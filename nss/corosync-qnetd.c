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
#include <signal.h>

#include "msg.h"
#include "msgio.h"
#include "tlv.h"
#include "nss-sock.h"
#include "qnetd-client.h"
#include "qnetd-clients-list.h"
#include "qnetd-poll-array.h"
#include "qnetd-log.h"
#include "dynar.h"
#include "timer-list.h"

#define QNETD_HOST      NULL
#define QNETD_PORT      4433
#define QNETD_LISTEN_BACKLOG	10
#define QNETD_MAX_CLIENT_SEND_SIZE	(1 << 15)
#define QNETD_MAX_CLIENT_RECEIVE_SIZE	(1 << 15)

#define NSS_DB_DIR	"nssdb"
#define QNETD_CERT_NICKNAME	"QNetd Cert"

#define QNETD_TLS_SUPPORTED			TLV_TLS_SUPPORTED
#define QNETD_TLS_CLIENT_CERT_REQUIRED		1

#define QNETD_HEARTBEAT_INTERVAL_MIN		1000
#define QNETD_HEARTBEAT_INTERVAL_MAX		200000

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
	int tls_client_cert_required;
};

/*
 * This is global variable used for comunication with main loop and signal (calls close)
 */
PRFileDesc *global_server_socket;

/*
 * Decision algorithms supported in this server
 */
#define QNETD_STATIC_SUPPORTED_DECISION_ALGORITHMS_SIZE		1

enum tlv_decision_algorithm_type
    qnetd_static_supported_decision_algorithms[QNETD_STATIC_SUPPORTED_DECISION_ALGORITHMS_SIZE] = {
	TLV_DECISION_ALGORITHM_TYPE_TEST,
};

static void
qnetd_err_nss(void) {

	qnetd_log_nss(LOG_CRIT, "NSS error");

	exit(1);
}

static void
qnetd_warn_nss(void) {

	qnetd_log_nss(LOG_WARNING, "NSS warning");
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
	case -4:
		qnetd_log(LOG_WARNING, "Received message with option with invalid value");
		break;
	default:
		qnetd_log(LOG_ERR, "Unknown error occured when decoding message");
		break;
	}
}

int
qnetd_client_net_schedule_send(struct qnetd_client *client)
{
	if (client->sending_msg) {
		/*
		 * Client is already sending msg
		 */
		return (-1);
	}

	client->msg_already_sent_bytes = 0;
	client->sending_msg = 1;

	return (0);
}

int
qnetd_client_send_err(struct qnetd_client *client, int add_msg_seq_number, uint32_t msg_seq_number,
    enum tlv_reply_error_code reply)
{

	if (msg_create_server_error(&client->send_buffer, add_msg_seq_number, msg_seq_number, reply) == 0) {
		qnetd_log(LOG_ERR, "Can't alloc server error msg. Disconnecting client connection.");

		return (-1);
	};

	if (qnetd_client_net_schedule_send(client) != 0) {
		qnetd_log(LOG_ERR, "Can't schedule send of error message. Disconnecting client connection.");

		return (-1);
	}

	return (0);
}

int
qnetd_client_msg_received_preinit(struct qnetd_instance *instance, struct qnetd_client *client,
	const struct msg_decoded *msg)
{

	if (msg->cluster_name == NULL) {
		qnetd_log(LOG_ERR, "Received preinit message without cluster name. Sending error reply.");

		if (qnetd_client_send_err(client, msg->seq_number_set, msg->seq_number,
		    TLV_REPLY_ERROR_CODE_DOESNT_CONTAIN_REQUIRED_OPTION) != 0) {
			return (-1);
		}

		return (0);
	}

	client->cluster_name = malloc(msg->cluster_name_len + 1);
	if (client->cluster_name == NULL) {
		qnetd_log(LOG_ERR, "Can't allocate cluster name. Sending error reply.");

		if (qnetd_client_send_err(client, msg->seq_number_set, msg->seq_number,
		    TLV_REPLY_ERROR_CODE_INTERNAL_ERROR) != 0) {
			return (-1);
		}

		return (0);
	}

	memcpy(client->cluster_name, msg->cluster_name, msg->cluster_name_len + 1);
	client->cluster_name_len = msg->cluster_name_len;
	client->preinit_received = 1;

	if (msg_create_preinit_reply(&client->send_buffer, msg->seq_number_set, msg->seq_number,
	    instance->tls_supported, instance->tls_client_cert_required) == 0) {
		qnetd_log(LOG_ERR, "Can't alloc preinit reply msg. Disconnecting client connection.");

		return (-1);
	};

	if (qnetd_client_net_schedule_send(client) != 0) {
		qnetd_log(LOG_ERR, "Can't schedule send of preinit message. Disconnecting client connection.");

		return (-1);
	}

	return (0);
}

int
qnetd_client_msg_received_preinit_reply(struct qnetd_instance *instance, struct qnetd_client *client,
	const struct msg_decoded *msg)
{

	qnetd_log(LOG_ERR, "Received preinit reply. Sending back error message");

	if (qnetd_client_send_err(client, msg->seq_number_set, msg->seq_number,
	    TLV_REPLY_ERROR_CODE_UNEXPECTED_MESSAGE) != 0) {
		return (-1);
	}

	return (0);
}

int
qnetd_client_msg_received_starttls(struct qnetd_instance *instance, struct qnetd_client *client,
	const struct msg_decoded *msg)
{
	PRFileDesc *new_pr_fd;

	if (!client->preinit_received) {
		qnetd_log(LOG_ERR, "Received starttls before preinit message. Sending error reply.");

		if (qnetd_client_send_err(client, msg->seq_number_set, msg->seq_number,
		    TLV_REPLY_ERROR_CODE_PREINIT_REQUIRED) != 0) {
			return (-1);
		}

		return (0);
	}

	if ((new_pr_fd = nss_sock_start_ssl_as_server(client->socket, instance->server.cert,
	    instance->server.private_key, instance->tls_client_cert_required, 0, NULL)) == NULL) {
		qnetd_log_nss(LOG_ERR, "Can't start TLS. Disconnecting client.");

		return (-1);
	}

	client->tls_started = 1;
	client->tls_peer_certificate_verified = 0;
	client->socket = new_pr_fd;

	return (0);
}

int
qnetd_client_msg_received_server_error(struct qnetd_instance *instance, struct qnetd_client *client,
	const struct msg_decoded *msg)
{
	qnetd_log(LOG_ERR, "Received server error. Sending back error message");

	if (qnetd_client_send_err(client, msg->seq_number_set, msg->seq_number,
	    TLV_REPLY_ERROR_CODE_UNEXPECTED_MESSAGE) != 0) {
		return (-1);
	}

	return (0);
}

/*
 *  0 - Success
 * -1 - Disconnect client
 * -2 - Error reply sent, but no need to disconnect client
 */
int
qnetd_client_check_tls(struct qnetd_instance *instance, struct qnetd_client *client, const struct msg_decoded *msg)
{
	int check_certificate;
	int tls_required;
	CERTCertificate *peer_cert;

	check_certificate = 0;
	tls_required = 0;

	switch (instance->tls_supported) {
	case TLV_TLS_UNSUPPORTED:
		tls_required = 0;
		check_certificate = 0;
		break;
	case TLV_TLS_SUPPORTED:
		tls_required = 0;

		if (client->tls_started && instance->tls_client_cert_required && !client->tls_peer_certificate_verified) {
			check_certificate = 1;
		}
	case TLV_TLS_REQUIRED:
		tls_required = 1;

		if (instance->tls_client_cert_required && !client->tls_peer_certificate_verified) {
			check_certificate = 1;
		}
		break;
	default:
		errx(1, "Unhandled instance tls supported %u\n", instance->tls_supported);
		break;
	}

	if (tls_required && !client->tls_started) {
		qnetd_log(LOG_ERR, "TLS is required but doesn't started yet. Sending back error message");

		if (qnetd_client_send_err(client, msg->seq_number_set, msg->seq_number,
		    TLV_REPLY_ERROR_CODE_TLS_REQUIRED) != 0) {
			return (-1);
		}

		return (-2);
	}

	if (check_certificate) {
		peer_cert = SSL_PeerCertificate(client->socket);

		if (peer_cert == NULL) {
			qnetd_log(LOG_ERR, "Client doesn't sent valid certificate. Disconnecting client");

			return (-1);
		}

		if (CERT_VerifyCertName(peer_cert, client->cluster_name) != SECSuccess) {
			qnetd_log(LOG_ERR, "Client doesn't sent certificate with valid CN. Disconnecting client");

			CERT_DestroyCertificate(peer_cert);

			return (-1);
		}

		CERT_DestroyCertificate(peer_cert);

		client->tls_peer_certificate_verified = 1;
	}

	return (0);
}

int
qnetd_client_msg_received_init(struct qnetd_instance *instance, struct qnetd_client *client,
	const struct msg_decoded *msg)
{
	int res;
	enum msg_type *supported_msgs;
	size_t no_supported_msgs;
	enum tlv_opt_type *supported_opts;
	size_t no_supported_opts;

	supported_msgs = NULL;
	supported_opts = NULL;
	no_supported_msgs = 0;
	no_supported_opts = 0;

	if ((res = qnetd_client_check_tls(instance, client, msg)) != 0) {
		return (res == -1 ? -1 : 0);
	}

	if (!client->preinit_received) {
		qnetd_log(LOG_ERR, "Received init before preinit message. Sending error reply.");

		if (qnetd_client_send_err(client, msg->seq_number_set, msg->seq_number,
		    TLV_REPLY_ERROR_CODE_PREINIT_REQUIRED) != 0) {
			return (-1);
		}

		return (0);
	}

	if (!msg->node_id_set) {
		qnetd_log(LOG_ERR, "Received init message without node id set. Sending error reply.");

		if (qnetd_client_send_err(client, msg->seq_number_set, msg->seq_number,
		    TLV_REPLY_ERROR_CODE_DOESNT_CONTAIN_REQUIRED_OPTION) != 0) {
			return (-1);
		}

		return (0);
	}

	if (msg->supported_messages != NULL) {
		/*
		 * Client sent supported messages. For now this is ignored but in the future
		 * this may be used to ensure backward compatibility.
		 */
/*
		for (i = 0; i < msg->no_supported_messages; i++) {
			qnetd_log(LOG_DEBUG, "Client supports %u message", (int)msg->supported_messages[i]);
		}
*/

		/*
		 * Sent back supported messages
		 */
		msg_get_supported_messages(&supported_msgs, &no_supported_msgs);
	}

	if (msg->supported_options != NULL) {
		/*
		 * Client sent supported options. For now this is ignored but in the future
		 * this may be used to ensure backward compatibility.
		 */
/*
		for (i = 0; i < msg->no_supported_options; i++) {
			qnetd_log(LOG_DEBUG, "Client supports %u option", (int)msg->supported_messages[i]);
		}
*/

		/*
		 * Send back supported options
		 */
		tlv_get_supported_options(&supported_opts, &no_supported_opts);
	}

	client->node_id_set = 1;
	client->node_id = msg->node_id;
	client->init_received = 1;

	if (msg_create_init_reply(&client->send_buffer, msg->seq_number_set, msg->seq_number,
	    supported_msgs, no_supported_msgs, supported_opts, no_supported_opts,
	    instance->max_client_receive_size, instance->max_client_send_size,
	    qnetd_static_supported_decision_algorithms, QNETD_STATIC_SUPPORTED_DECISION_ALGORITHMS_SIZE) == -1) {
		qnetd_log(LOG_ERR, "Can't alloc init reply msg. Disconnecting client connection.");

		return (-1);
	}

	if (qnetd_client_net_schedule_send(client) != 0) {
		qnetd_log(LOG_ERR, "Can't schedule send of init reply message. Disconnecting client connection.");

		return (-1);
	}

	return (0);
}

int
qnetd_client_msg_received_init_reply(struct qnetd_instance *instance, struct qnetd_client *client,
	const struct msg_decoded *msg)
{
	qnetd_log(LOG_ERR, "Received init reply. Sending back error message");

	if (qnetd_client_send_err(client, msg->seq_number_set, msg->seq_number,
	    TLV_REPLY_ERROR_CODE_UNEXPECTED_MESSAGE) != 0) {
		return (-1);
	}

	return (0);
}

int
qnetd_client_msg_received_set_option_reply(struct qnetd_instance *instance, struct qnetd_client *client,
	const struct msg_decoded *msg)
{
	qnetd_log(LOG_ERR, "Received set option reply. Sending back error message");

	if (qnetd_client_send_err(client, msg->seq_number_set, msg->seq_number,
	    TLV_REPLY_ERROR_CODE_UNEXPECTED_MESSAGE) != 0) {
		return (-1);
	}

	return (0);
}

int
qnetd_client_msg_received_set_option(struct qnetd_instance *instance, struct qnetd_client *client,
	const struct msg_decoded *msg)
{
	int res;
	size_t zi;

	if ((res = qnetd_client_check_tls(instance, client, msg)) != 0) {
		return (res == -1 ? -1 : 0);
	}

	if (!client->init_received) {
		qnetd_log(LOG_ERR, "Received set option message before init message. Sending error reply.");

		if (qnetd_client_send_err(client, msg->seq_number_set, msg->seq_number,
		    TLV_REPLY_ERROR_CODE_INIT_REQUIRED) != 0) {
			return (-1);
		}

		return (0);
	}

	if (msg->decision_algorithm_set) {
		/*
		 * Check if decision algorithm requested by client is supported
		 */
		res = 0;

		for (zi = 0; zi < QNETD_STATIC_SUPPORTED_DECISION_ALGORITHMS_SIZE && !res; zi++) {
			if (qnetd_static_supported_decision_algorithms[zi] == msg->decision_algorithm) {
				res = 1;
			}
		}

		if (!res) {
			qnetd_log(LOG_ERR, "Client requested unsupported decision algorithm %u. Sending error reply.",
			    msg->decision_algorithm);

			if (qnetd_client_send_err(client, msg->seq_number_set, msg->seq_number,
			    TLV_REPLY_ERROR_CODE_UNSUPPORTED_DECISION_ALGORITHM) != 0) {
				return (-1);
			}

			return (0);
		}

		client->decision_algorithm = msg->decision_algorithm;
	}

	if (msg->heartbeat_interval_set) {
		/*
		 * Check if heartbeat interval is valid
		 */
		if (msg->heartbeat_interval != 0 && (msg->heartbeat_interval < QNETD_HEARTBEAT_INTERVAL_MIN ||
		    msg->heartbeat_interval > QNETD_HEARTBEAT_INTERVAL_MAX)) {
			qnetd_log(LOG_ERR, "Client requested invalid heartbeat interval %u. Sending error reply.",
			    msg->heartbeat_interval);

			if (qnetd_client_send_err(client, msg->seq_number_set, msg->seq_number,
			    TLV_REPLY_ERROR_CODE_INVALID_HEARTBEAT_INTERVAL) != 0) {
				return (-1);
			}

			return (0);
		}

		client->heartbeat_interval = msg->heartbeat_interval;
	}

	if (msg_create_set_option_reply(&client->send_buffer, msg->seq_number_set, msg->seq_number,
	    client->decision_algorithm, client->heartbeat_interval) == -1) {
		qnetd_log(LOG_ERR, "Can't alloc set option reply msg. Disconnecting client connection.");

		return (-1);
	}

	if (qnetd_client_net_schedule_send(client) != 0) {
		qnetd_log(LOG_ERR, "Can't schedule send of set option reply message. Disconnecting client connection.");

		return (-1);
	}

	return (0);
}

int
qnetd_client_msg_received_echo_reply(struct qnetd_instance *instance, struct qnetd_client *client,
	const struct msg_decoded *msg)
{
	qnetd_log(LOG_ERR, "Received echo reply. Sending back error message");

	if (qnetd_client_send_err(client, msg->seq_number_set, msg->seq_number,
	    TLV_REPLY_ERROR_CODE_UNEXPECTED_MESSAGE) != 0) {
		return (-1);
	}

	return (0);
}

int
qnetd_client_msg_received_echo_request(struct qnetd_instance *instance, struct qnetd_client *client,
	const struct msg_decoded *msg, const struct dynar *msg_orig)
{
	int res;

	if ((res = qnetd_client_check_tls(instance, client, msg)) != 0) {
		return (res == -1 ? -1 : 0);
	}

	if (!client->init_received) {
		qnetd_log(LOG_ERR, "Received echo request before init message. Sending error reply.");

		if (qnetd_client_send_err(client, msg->seq_number_set, msg->seq_number,
		    TLV_REPLY_ERROR_CODE_INIT_REQUIRED) != 0) {
			return (-1);
		}

		return (0);
	}

	if (msg_create_echo_reply(&client->send_buffer, msg_orig) == -1) {
		qnetd_log(LOG_ERR, "Can't alloc echo reply msg. Disconnecting client connection.");

		return (-1);
	}

	if (qnetd_client_net_schedule_send(client) != 0) {
		qnetd_log(LOG_ERR, "Can't schedule send of echo reply message. Disconnecting client connection.");

		return (-1);
	}

	return (0);
}

int
qnetd_client_msg_received(struct qnetd_instance *instance, struct qnetd_client *client)
{
	struct msg_decoded msg;
	int res;
	int ret_val;

	msg_decoded_init(&msg);

	res = msg_decode(&client->receive_buffer, &msg);
	if (res != 0) {
		/*
		 * Error occurred. Send server error.
		 */
		qnetd_client_log_msg_decode_error(res);
		qnetd_log(LOG_INFO, "Sending back error message");

		if (qnetd_client_send_err(client, msg.seq_number_set, msg.seq_number,
		    TLV_REPLY_ERROR_CODE_ERROR_DECODING_MSG) != 0) {
			return (-1);
		}

		return (0);
	}

	ret_val = 0;

	switch (msg.type) {
	case MSG_TYPE_PREINIT:
		ret_val = qnetd_client_msg_received_preinit(instance, client, &msg);
		break;
	case MSG_TYPE_PREINIT_REPLY:
		ret_val = qnetd_client_msg_received_preinit_reply(instance, client, &msg);
		break;
	case MSG_TYPE_STARTTLS:
		ret_val = qnetd_client_msg_received_starttls(instance, client, &msg);
		break;
	case MSG_TYPE_INIT:
		ret_val = qnetd_client_msg_received_init(instance, client, &msg);
		break;
	case MSG_TYPE_INIT_REPLY:
		ret_val = qnetd_client_msg_received_init_reply(instance, client, &msg);
		break;
	case MSG_TYPE_SERVER_ERROR:
		ret_val = qnetd_client_msg_received_server_error(instance, client, &msg);
		break;
	case MSG_TYPE_SET_OPTION:
		ret_val = qnetd_client_msg_received_set_option(instance, client, &msg);
		break;
	case MSG_TYPE_SET_OPTION_REPLY:
		ret_val = qnetd_client_msg_received_set_option_reply(instance, client, &msg);
		break;
	case MSG_TYPE_ECHO_REQUEST:
		ret_val = qnetd_client_msg_received_echo_request(instance, client, &msg, &client->receive_buffer);
		break;
	case MSG_TYPE_ECHO_REPLY:
		ret_val = qnetd_client_msg_received_echo_reply(instance, client, &msg);
		break;
	default:
		qnetd_log(LOG_ERR, "Unsupported message %u received from client. Sending back error message",
		    msg.type);

		if (qnetd_client_send_err(client, msg.seq_number_set, msg.seq_number,
		    TLV_REPLY_ERROR_CODE_UNSUPPORTED_MESSAGE) != 0) {
			ret_val = -1;
		}

		break;
	}

	msg_decoded_destroy(&msg);

	return (ret_val);
}

int
qnetd_client_net_write_finished(struct qnetd_instance *instance, struct qnetd_client *client)
{

	/*
	 * Callback is currently unused
	 */

	return (0);
}

int
qnetd_client_net_write(struct qnetd_instance *instance, struct qnetd_client *client)
{
	int res;

	res = msgio_write(client->socket, &client->send_buffer, &client->msg_already_sent_bytes);

	if (res == 1) {
		client->sending_msg = 0;

		if (qnetd_client_net_write_finished(instance, client) == -1) {
			return (-1);
		}
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
 * -1 means end of connection (EOF) or some other unhandled error. 0 = success
 */
int
qnetd_client_net_read(struct qnetd_instance *instance, struct qnetd_client *client)
{
	int res;
	int ret_val;
	int orig_skipping_msg;

	orig_skipping_msg = client->skipping_msg;

	res = msgio_read(client->socket, &client->receive_buffer, &client->msg_already_received_bytes,
	    &client->skipping_msg);

	if (!orig_skipping_msg && client->skipping_msg) {
		qnetd_log(LOG_DEBUG, "msgio_read set skipping_msg");
	}

	ret_val = 0;

	switch (res) {
	case 0:
		/*
		 * Partial read
		 */
		break;
	case -1:
		qnetd_log(LOG_DEBUG, "Client closed connection");
		ret_val = -1;
		break;
	case -2:
		qnetd_log_nss(LOG_ERR, "Unhandled error when reading from client. Disconnecting client");
		ret_val = -1;
		break;
	case -3:
		qnetd_log(LOG_ERR, "Can't store message header from client. Disconnecting client");
		ret_val = -1;
		break;
	case -4:
		qnetd_log(LOG_ERR, "Can't store message from client. Skipping message");
		client->skipping_msg_reason = TLV_REPLY_ERROR_CODE_ERROR_DECODING_MSG;
		break;
	case -5:
		qnetd_log(LOG_WARNING, "Client sent unsupported msg type %u. Skipping message",
			    msg_get_type(&client->receive_buffer));
		client->skipping_msg_reason = TLV_REPLY_ERROR_CODE_UNSUPPORTED_MESSAGE;
		break;
	case -6:
		qnetd_log(LOG_WARNING,
		    "Client wants to send too long message %u bytes. Skipping message",
		    msg_get_len(&client->receive_buffer));
		client->skipping_msg_reason = TLV_REPLY_ERROR_CODE_MESSAGE_TOO_LONG;
		break;
	case 1:
		/*
		 * Full message received / skipped
		 */
		if (!client->skipping_msg) {
			if (qnetd_client_msg_received(instance, client) == -1) {
				ret_val = -1;
			}
		} else {
			if (qnetd_client_send_err(client, 0, 0, client->skipping_msg_reason) != 0) {
				ret_val = -1;
			}
		}

		client->skipping_msg = 0;
		client->skipping_msg_reason = TLV_REPLY_ERROR_CODE_NO_ERROR;
		client->msg_already_received_bytes = 0;
		dynar_clean(&client->receive_buffer);
		break;
	default:
		errx(1, "Unhandled msgio_read error %d\n", res);
		break;
	}

	return (ret_val);
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
qnetd_client_disconnect(struct qnetd_instance *instance, struct qnetd_client *client)
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
	int client_disconnect;

	client = NULL;
	client_disconnect = 0;

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

			client_disconnect = 0;

			if (!client_disconnect && pfds[i].out_flags & PR_POLL_READ) {
				if (i == 0) {
					qnetd_client_accept(instance);
				} else {
					if (qnetd_client_net_read(instance, client) == -1) {
						client_disconnect = 1;
					}
				}
			}

			if (!client_disconnect && pfds[i].out_flags & PR_POLL_WRITE) {
				if (i == 0) {
					/*
					 * Poll write on listen socket -> fatal error
					 */
					qnetd_log(LOG_CRIT, "POLL_WRITE on listening socket");

					return (-1);
				} else {
					if (qnetd_client_net_write(instance, client) == -1) {
						client_disconnect = 1;
					}
				}
			}

			if (!client_disconnect &&
			    pfds[i].out_flags & (PR_POLL_ERR|PR_POLL_NVAL|PR_POLL_HUP|PR_POLL_EXCEPT)) {
				if (i == 0) {
					if (pfds[i].out_flags != PR_POLL_NVAL) {
						/*
						 * Poll ERR on listening socket is fatal error. POLL_NVAL is
						 * used as a signal to quit poll loop.
						 */
						qnetd_log(LOG_CRIT, "POLL_ERR (%u) on listening socket", pfds[i].out_flags);
					} else {
						qnetd_log(LOG_DEBUG, "Listening socket is closed");
					}

					return (-1);

				} else {
					qnetd_log(LOG_DEBUG, "POLL_ERR (%u) on client socket. Disconnecting.",
					    pfds[i].out_flags);

					client_disconnect = 1;
				}
			}

			/*
			 * If client is scheduled for disconnect, disconnect it
			 */
			if (client_disconnect) {
				qnetd_client_disconnect(instance, client);
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
    size_t max_client_send_size, enum tlv_tls_supported tls_supported, int tls_client_cert_required)
{

	memset(instance, 0, sizeof(*instance));

	qnetd_poll_array_init(&instance->poll_array);
	qnetd_clients_list_init(&instance->clients);

	instance->max_client_receive_size = max_client_receive_size;
	instance->max_client_send_size = max_client_send_size;

	instance->tls_supported = tls_supported;
	instance->tls_client_cert_required = tls_client_cert_required;

	return (0);
}

int
qnetd_instance_destroy(struct qnetd_instance *instance)
{
	struct qnetd_client *client;
	struct qnetd_client *client_next;

	client = TAILQ_FIRST(&instance->clients);
	while (client != NULL) {
		client_next = TAILQ_NEXT(client, entries);

		qnetd_client_disconnect(instance, client);

		client = client_next;
	}

	qnetd_poll_array_destroy(&instance->poll_array);
	qnetd_clients_list_free(&instance->clients);

	return (0);
}

static void
signal_int_handler(int sig)
{

	qnetd_log(LOG_DEBUG, "SIGINT received - closing server socket");

	PR_Close(global_server_socket);
}

void
signal_handlers_register(void)
{
	struct sigaction act;

	act.sa_handler = signal_int_handler;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_RESTART;

	sigaction(SIGINT, &act, NULL);
}

int
main(void)
{
	struct qnetd_instance instance;

	/*
	 * INIT
	 */
	qnetd_log_init(QNETD_LOG_TARGET_STDERR);
	qnetd_log_set_debug(1);

	if (nss_sock_init_nss(NSS_DB_DIR) != 0) {
		qnetd_err_nss();
	}

	if (SSL_ConfigServerSessionIDCache(0, 0, 0, NULL) != SECSuccess) {
		qnetd_err_nss();
	}

	if (qnetd_instance_init(&instance, QNETD_MAX_CLIENT_RECEIVE_SIZE, QNETD_MAX_CLIENT_SEND_SIZE,
	    QNETD_TLS_SUPPORTED, QNETD_TLS_CLIENT_CERT_REQUIRED) == -1) {
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

	global_server_socket = instance.server.socket;
	signal_handlers_register();

	/*
	 * MAIN LOOP
	 */
	while (qnetd_poll(&instance) == 0) {
	}

	/*
	 * Cleanup
	 */
	CERT_DestroyCertificate(instance.server.cert);
	SECKEY_DestroyPrivateKey(instance.server.private_key);

	SSL_ClearSessionCache();

	SSL_ShutdownServerSessionIDCache();

	qnetd_instance_destroy(&instance);

	if (NSS_Shutdown() != SECSuccess) {
		qnetd_warn_nss();
	}

	if (PR_Cleanup() != PR_SUCCESS) {
		qnetd_warn_nss();
	}

	qnetd_log_close();

	return (0);
}
