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
#include "msgio.h"
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

#define QNETD_CLIENT_NET_LOCAL_NET_BUFFER	(16)

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
	enum tlv_tls_supported tls_supported;
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

int
qnetd_client_net_schedule_send(struct qnetd_client *client)
{
	if (client->conn_state != QNETD_CLIENT_CONN_STATE_RECEIVING_MSG_VALUE &&
	    client->conn_state != QNETD_CLIENT_CONN_STATE_SKIPPING_MSG) {
		/*
		 * Client is already sending msg, doing handshake or it doesn't received
		 * full header
		 */
		return (-1);
	}

	client->msg_already_sent_bytes = 0;
	client->conn_state = QNETD_CLIENT_CONN_STATE_SENDING_MSG;

	return (0);
}

void
qnetd_client_msg_received(struct qnetd_instance *instance, struct qnetd_client *client)
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
		if (msg_create_preinit_reply(&client->send_buffer, msg.seq_number_set, msg.seq_number,
		    instance->tls_supported) == 0) {
			qnetd_log(LOG_ERR, "Can't alloc preinit reply msg. Terminating client connection.");

			// TODO
		};

		if (qnetd_client_net_schedule_send(client) != 0) {
			qnetd_log(LOG_ERR, "Can't schedule send of message");
		}

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


int
qnetd_client_net_write(struct qnetd_instance *instance, struct qnetd_client *client)
{
	int res;

	res = msgio_write(client->socket, &client->send_buffer, &client->msg_already_sent_bytes);

	if (res == 1) {
		client->conn_state = QNETD_CLIENT_CONN_STATE_RECEIVING_MSG_HEADER;

	}

	if (res == -1) {
		qnetd_log_nss(LOG_CRIT, "PR_Send returned 0");

		return (-1);
	}

	if (res == -2) {
		qnetd_log_nss(LOG_ERR, "Unhandled error when sending message to client");

		return (-1);
	}

	return (0);
}

/*
 * -1 End of connection
 * -2 Unhandled error
 * -3 Fatal error. Unable to store message header
 * -4 Unable to store message
 * -5 Invalid msg type
 * -6 Msg too long
 */
int
msgio_read(PRFileDesc *socket, struct dynar *msg, size_t *already_received_bytes, int *skipping_msg)
{
	char local_read_buffer[QNETD_CLIENT_NET_LOCAL_NET_BUFFER];
	PRInt32 readed;
	PRInt32 to_read;
	int ret;

	ret = 0;

	if (*already_received_bytes < msg_get_header_length()) {
		/*
		 * Complete reading of header
		 */
		to_read = msg_get_header_length() - *already_received_bytes;
	} else {
		/*
		 * Read rest of message (or at least as much as possible)
		 */
		to_read = (msg_get_header_length() + msg_get_len(msg)) - *already_received_bytes;
	}

	if (to_read > QNETD_CLIENT_NET_LOCAL_NET_BUFFER) {
		to_read = QNETD_CLIENT_NET_LOCAL_NET_BUFFER;
	}

	readed = PR_Recv(socket, local_read_buffer, to_read, 0, PR_INTERVAL_NO_TIMEOUT);
	if (readed > 0) {
		*already_received_bytes += readed;

		if (!*skipping_msg) {
			if (dynar_cat(msg, local_read_buffer, readed) == -1) {
				*skipping_msg = 1;
				ret = -4;
			}
		}

		if (*skipping_msg && *already_received_bytes < msg_get_header_length()) {
			/*
			 * Fatal error. We were unable to store even message header
			 */
			return (-3);
		}

		if (!*skipping_msg && *already_received_bytes == msg_get_header_length()) {
			/*
			 * Full header received. Check type, maximum size, ...
			 */
			if (!msg_is_valid_msg_type(msg)) {
				*skipping_msg = 1;
				ret = -5;
			} else if (msg_get_header_length() + msg_get_len(msg) > dynar_max_size(msg)) {
				*skipping_msg = 1;
				ret = -6;
			}
		}

		if (*already_received_bytes >= msg_get_header_length() &&
		    *already_received_bytes == (msg_get_header_length() + msg_get_len(msg))) {
			/*
			 * Full message skipped or received
			 */
			ret = 1;
		}

	}

	if (readed == 0) {
		return (-1);
	}

	if (readed < 0 && PR_GetError() != PR_WOULD_BLOCK_ERROR) {
		return (-2);
	}

	return (ret);
}

/*
 * -1 means end of connection (EOF) or some other unhandled error. 0 = success
 */
int
qnetd_client_net_read(struct qnetd_instance *instance, struct qnetd_client *client)
{
	int res;
	int skipping_msg;

	skipping_msg = (client->conn_state == QNETD_CLIENT_CONN_STATE_SKIPPING_MSG);

	res = msgio_read(client->socket, &client->receive_buffer, &client->msg_already_received_bytes, &skipping_msg);

	if (skipping_msg) {
		fprintf(stderr, "SKIPPING MSG\n");

		client->conn_state = QNETD_CLIENT_CONN_STATE_SKIPPING_MSG;
	}

	if (res == -1) {
		return (-1);
	}

	if (res == -2) {
		qnetd_log_nss(LOG_ERR, "Unhandled error when reading from client");

		return (-1);
	}

	if (res == -3) {
		qnetd_log(LOG_ERR, "Can't store message header from client. Terminating client");

		return (-1);
	}

	if (res == -4) {
		qnetd_log(LOG_ERR, "Can't store message from client. Skipping message");
	}

	if (res == -5) {
		qnetd_log(LOG_WARNING, "Client sent unsupported msg type %u. Skipping message",
			    msg_get_type(&client->receive_buffer));
	}

	if (res == -6) {
		qnetd_log(LOG_WARNING,
		    "Client wants to send too long message %u bytes. Skipping message",
		    msg_get_len(&client->receive_buffer));
	}

	if (res == 1) {
		/*
		 * Full message received
		 */
		if (!skipping_msg) {
			fprintf(stderr, "FULL MESSAGE RECEIVED\n");
			qnetd_client_msg_received(instance, client);
		} else {
			fprintf(stderr, "FULL MESSAGE SKIPPED\n");
		}
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

	PR_Close(client->socket);
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
	int terminate_client;

	client = NULL;
	terminate_client = 0;

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
					if (qnetd_client_net_read(instance, client) == -1) {
						fprintf(stderr, "Client terminate 1\n");
						terminate_client = 1;
					}
				}
			}

			if (pfds[i].out_flags & PR_POLL_WRITE) {
				if (i == 0) {
					/*
					 * Poll write on listen socket -> fatal error
					 */
					qnetd_log(LOG_CRIT, "POLL_WRITE on listening socket");

					return (-1);
				} else {
					fprintf(stderr, "net write");
					if (qnetd_client_net_write(instance, client) == -1) {
						fprintf(stderr, "Client terminate 2 \n");
						terminate_client = 1;
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
					fprintf(stderr, "Client terminate 3\n");

					terminate_client = 1;
				}
			}

			/*
			 * If client is scheduled for termination, terminate it
			 */
			if (terminate_client) {
				qnetd_client_terminate(instance, client);
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
