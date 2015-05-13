#include <stdio.h>
#include <nss.h>
#include <pk11func.h>
#include <certt.h>
#include <ssl.h>
#include <prio.h>
#include <prnetdb.h>
#include <prerror.h>
#include <prinit.h>
#include <getopt.h>
#include <err.h>
#include <keyhi.h>
#include <syslog.h>

#include "msg.h"
#include "tlv.h"
#include "nss-sock.h"
#include "qnetd-client.h"
#include "qnetd-clients-list.h"
#include "qnetd-poll-array.h"
#include "qnetd-log.h"
#include "dynar.h"

#define QNETD_HOST      NULL
#define QNETD_PORT      4433
#define QNETD_LISTEN_BACKLOG	10
#define QNETD_MAX_CLIENT_SEND_SIZE	(1 << 15)
#define QNETD_MAX_CLIENT_RECEIVE_SIZE	(1 << 15)

#define NSS_DB_DIR	"nssdb"
#define QNETD_CERT_NICKNAME	"QNetd Cert"

#define QNETD_CLIENT_NET_LOCAL_READ_BUFFER	(1 << 10)

struct qnetd_instance {
	struct {
		PRFileDesc *socket;
		CERTCertificate *cert;
		SECKEYPrivateKey *private_key;
	} server;
	size_t max_client_receive_size;
	size_t max_client_send_size;
	struct qnetd_clients_list clients;
	struct qnetd_poll_array poll_array;
};



static void qnetd_err_nss(void) {
	qnetd_log_nss(LOG_CRIT, "NSS error");

	exit(1);
}

void
qnetd_client_log_msg_decode_error(int ret)
{

	switch (ret) {
	case -1:
		qnetd_log(LOG_WARNING, "Received message with option with invalid length");
		break;
	case -2:
		qnetd_log(LOG_CRIT, "Can't allocate memory");
		break;
	case -3:
		qnetd_log(LOG_WARNING, "Received inconsistent msg (tlv len > msg size)");
		break;
	default:
		qnetd_log(LOG_ERR, "Unknown error occured when decoding message");
		break;
	}
}

void
qnetd_client_msg_received(struct qnetd_client *client)
{
	struct msg_decoded msg;
	int res;

	msg_decoded_init(&msg);

	res = msg_decode(&client->receive_buffer, &msg);
	if (res != 0) {
		/*
		 * Error occurred. Send server error.
		 */
		qnetd_client_log_msg_decode_error(res);
		qnetd_log(LOG_INFO, "Sending back error message");
	}

	switch (msg.type) {
	case MSG_TYPE_PREINIT:
		fprintf(stderr, "PREINIT\n");
		break;
	case MSG_TYPE_PREINIT_REPLY:
		fprintf(stderr, "PREINIT reply\n");
		break;
	default:
		fprintf(stderr, "Unsupported message received\n");
		break;
	}

	msg_decoded_destroy(&msg);
}

/*
 * -1 means end of connection (EOF) or some other unhandled error. 0 = success
 */
int
qnetd_client_net_read(struct qnetd_client *client)
{
	char local_read_buffer[QNETD_CLIENT_NET_LOCAL_READ_BUFFER];
	PRInt32 readed;
	PRInt32 to_read;

	if (client->msg_already_received_bytes < msg_get_header_length()) {
		/*
		 * Complete reading of header
		 */
		to_read = msg_get_header_length() - client->msg_already_received_bytes;
	} else {
		/*
		 * Read rest of message (or at least as much as possible)
		 */
		to_read = (msg_get_header_length() + msg_get_len(&client->receive_buffer)) -
		    client->msg_already_received_bytes;
		if (to_read > QNETD_CLIENT_NET_LOCAL_READ_BUFFER) {
			to_read = QNETD_CLIENT_NET_LOCAL_READ_BUFFER;
		}
	}

	readed = PR_Recv(client->socket, local_read_buffer, to_read, 0, PR_INTERVAL_NO_TIMEOUT);
	if (readed > 0) {
		client->msg_already_received_bytes += readed;

		if (client->conn_state == QNETD_CLIENT_CONN_STATE_RECEIVING_MSG_HEADER ||
		    client->conn_state == QNETD_CLIENT_CONN_STATE_RECEIVING_MSG_VALUE) {
			if (dynar_cat(&client->receive_buffer, local_read_buffer, readed) == -1) {
				qnetd_log(LOG_ERR, "Can't store message from client. Skipping message");
				client->conn_state = QNETD_CLIENT_CONN_STATE_SKIPPING_MSG;
			}
		}

		if (client->conn_state == QNETD_CLIENT_CONN_STATE_RECEIVING_MSG_HEADER) {
			if (client->msg_already_received_bytes == msg_get_header_length()) {
				client->conn_state = QNETD_CLIENT_CONN_STATE_RECEIVING_MSG_VALUE;

				/*
				 * Full header received. Check type, maximum size, ...
				 */
				if (!msg_is_valid_msg_type(&client->receive_buffer)) {
					qnetd_log(LOG_WARNING, "Client sent unsupported msg type %u. Skipping message",
					    msg_get_type(&client->receive_buffer));

					client->conn_state = QNETD_CLIENT_CONN_STATE_SKIPPING_MSG;
				}

				if (msg_get_header_length() + msg_get_len(&client->receive_buffer) >
				    dynar_max_size(&client->receive_buffer)) {
					qnetd_log(LOG_WARNING,
					    "Client wants to send too long message %u bytes. Skipping message",
					    msg_get_len(&client->receive_buffer));

					client->conn_state = QNETD_CLIENT_CONN_STATE_SKIPPING_MSG;
				}
			}
		} else {
			if (client->msg_already_received_bytes ==
			    (msg_get_header_length() + msg_get_len(&client->receive_buffer))) {
				/*
				 * Full message received
				 */
				fprintf(stderr, "FULL MESSAGE RECEIVED\n");
				qnetd_client_msg_received(client);
			}
		}
	}

	if (readed == 0) {
		return (-1);
	}

	if (readed < 0 && PR_GetError() != PR_WOULD_BLOCK_ERROR) {
		qnetd_log_nss(LOG_ERR, "Unhandled error when reading from client");

		return (-1);
	}

	return (0);
}

int
qnetd_client_accept(struct qnetd_instance *instance)
{
	PRNetAddr client_addr;
	PRFileDesc *client_socket;
	struct qnetd_client *client;

        if ((client_socket = PR_Accept(instance->server.socket, &client_addr, PR_INTERVAL_NO_TIMEOUT)) == NULL) {
		qnetd_log_nss(LOG_ERR, "Can't accept connection");
		return (-1);
	}

	if (nss_sock_set_nonblocking(client_socket) != 0) {
		qnetd_log_nss(LOG_ERR, "Can't set client socket to non blocking mode");
		return (-1);
	}

	client = qnetd_clients_list_add(&instance->clients, client_socket, &client_addr,
	    instance->max_client_receive_size, instance->max_client_send_size);
	if (client == NULL) {
		qnetd_log(LOG_ERR, "Can't add client to list");
		return (-2);
	}

	return (0);
}

void
qnetd_client_terminate(struct qnetd_instance *instance, struct qnetd_client *client)
{

	qnetd_clients_list_del(&instance->clients, client);
}

int
qnetd_poll(struct qnetd_instance *instance)
{
	struct qnetd_client *client;
	struct qnetd_client *client_next;
	PRPollDesc *pfds;
	PRInt32 poll_res;
	int i;

	client = NULL;

	pfds = qnetd_poll_array_create_from_clients_list(&instance->poll_array,
	    &instance->clients, instance->server.socket, PR_POLL_READ);

	if (pfds == NULL) {
		return (-1);
	}

	if ((poll_res = PR_Poll(pfds, qnetd_poll_array_size(&instance->poll_array),
	    PR_INTERVAL_NO_TIMEOUT)) > 0) {
		/*
		 * Walk thru pfds array and process events
		 */
		for (i = 0; i < qnetd_poll_array_size(&instance->poll_array); i++) {
			/*
			 * Also traverse clients list
			 */
			if (i > 0) {
				if (i == 1) {
					client = TAILQ_FIRST(&instance->clients);
					client_next = TAILQ_NEXT(client, entries);
				} else {
					client = client_next;
					client_next = TAILQ_NEXT(client, entries);
				}
			}

			if (pfds[i].out_flags & PR_POLL_READ) {
				if (i == 0) {
					qnetd_client_accept(instance);
				} else {
					if (qnetd_client_net_read(client) == -1) {
						qnetd_client_terminate(instance, client);
					}
				}
			}

			if (pfds[i].out_flags & (PR_POLL_ERR|PR_POLL_NVAL|PR_POLL_HUP|PR_POLL_EXCEPT)) {
				if (i == 0) {
					/*
					 * Poll ERR on listening socket is fatal error
					 */
					qnetd_log(LOG_CRIT, "POLL_ERR (%u) on listening socket", pfds[i].out_flags);
					return (-1);

				} else {
					qnetd_client_terminate(instance, client);
				}
			}
		}
	}

	return (0);
}

int
qnetd_instance_init_certs(struct qnetd_instance *instance)
{

	instance->server.cert = PK11_FindCertFromNickname(QNETD_CERT_NICKNAME, NULL);
	if (instance->server.cert == NULL) {
		return (-1);
	}

	instance->server.private_key = PK11_FindKeyByAnyCert(instance->server.cert, NULL);
	if (instance->server.private_key == NULL) {
		return (-1);
	}

	return (0);
}

int
qnetd_instance_init(struct qnetd_instance *instance, size_t max_client_receive_size,
    size_t max_client_send_size)
{

	memset(instance, 0, sizeof(*instance));

	qnetd_poll_array_init(&instance->poll_array);
	qnetd_clients_list_init(&instance->clients);

	instance->max_client_receive_size = max_client_receive_size;
	instance->max_client_send_size = max_client_send_size;

	return (0);
}


int main(void)
{
	struct qnetd_instance instance;

	qnetd_log_init(QNETD_LOG_TARGET_STDERR);

	if (nss_sock_init_nss(NSS_DB_DIR) != 0) {
		qnetd_err_nss();
	}

	if (SSL_ConfigServerSessionIDCache(0, 0, 0, NULL) != SECSuccess) {
		qnetd_err_nss();
	}

	if (qnetd_instance_init(&instance, QNETD_MAX_CLIENT_RECEIVE_SIZE, QNETD_MAX_CLIENT_SEND_SIZE) == -1) {
		errx(1, "Can't initialize qnetd");
	}

	if (qnetd_instance_init_certs(&instance) == -1) {
		qnetd_err_nss();
	}

	instance.server.socket = nss_sock_create_listen_socket(QNETD_HOST, QNETD_PORT, PR_AF_INET6);
	if (instance.server.socket == NULL) {
		qnetd_err_nss();
	}

	if (nss_sock_set_nonblocking(instance.server.socket) != 0) {
		qnetd_err_nss();
	}

	if (PR_Listen(instance.server.socket, QNETD_LISTEN_BACKLOG) != PR_SUCCESS) {
		qnetd_err_nss();
	}

	while (1) {
		qnetd_poll(&instance);
		fprintf(stderr,"POLL\n");
	}

	PR_Close(instance.server.socket);
	CERT_DestroyCertificate(instance.server.cert);
	SECKEY_DestroyPrivateKey(instance.server.private_key);

	SSL_ClearSessionCache();

	SSL_ShutdownServerSessionIDCache();

	if (NSS_Shutdown() != SECSuccess) {
		qnetd_err_nss();
	}

	PR_Cleanup();

	qnetd_log_close();

	return (0);
}
