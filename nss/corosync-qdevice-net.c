#include <stdio.h>
#include <nss.h>
#include <secerr.h>
#include <sslerr.h>
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

#include "dynar.h"
#include "nss-sock.h"
#include "tlv.h"
#include "msg.h"
#include "msgio.h"
#include "qnetd-log.h"

#define NSS_DB_DIR	"node/nssdb"

#define QNETD_HOST	"localhost"
#define QNETD_PORT	4433

#define QDEVICE_NET_MAX_MSG_RECEIVE_SIZE	(1 << 15)
#define QDEVICE_NET_MAX_MSG_SEND_SIZE		(1 << 15)

#define QDEVICE_NET_TLS_SUPPORTED	TLV_TLS_SUPPORTED

#define qdevice_net_log			qnetd_log
#define qdevice_net_log_nss		qnetd_log_nss
#define qdevice_net_log_init		qnetd_log_init
#define qdevice_net_log_close		qnetd_log_close
#define qdevice_net_log_set_debug	qnetd_log_set_debug

#define QDEVICE_NET_LOG_TARGET_STDERR		QNETD_LOG_TARGET_STDERR
#define QDEVICE_NET_LOG_TARGET_SYSLOG		QNETD_LOG_TARGET_SYSLOG

enum qdevice_net_state {
	QDEVICE_NET_STATE_WAITING_PREINIT_REPLY,
};

struct qdevice_net_instance {
	PRFileDesc *socket;
	size_t max_send_size;
	size_t max_receive_size;
	struct dynar receive_buffer;
	struct dynar send_buffer;
	int sending_msg;
	int skipping_msg;
	size_t msg_already_received_bytes;
	size_t msg_already_sent_bytes;
	enum qdevice_net_state state;
	uint32_t expected_msg_seq_num;
	enum tlv_tls_supported tls_supported;
	int using_tls;
};

static void
err_nss(void) {
	errx(1, "nss error %d: %s", PR_GetError(), PR_ErrorToString(PR_GetError(), PR_LANGUAGE_I_DEFAULT));
}

void
qdevice_net_log_msg_decode_error(int ret)
{

	switch (ret) {
	case -1:
		qdevice_net_log(LOG_WARNING, "Received message with option with invalid length");
		break;
	case -2:
		qdevice_net_log(LOG_CRIT, "Can't allocate memory");
		break;
	case -3:
		qdevice_net_log(LOG_WARNING, "Received inconsistent msg (tlv len > msg size)");
		break;
	case -4:
		qdevice_net_log(LOG_ERR, "Received message with option with invalid value");
		break;
	default:
		qdevice_net_log(LOG_ERR, "Unknown error occured when decoding message");
		break;
	}
}

/*
 * -1 - Incompatible tls combination
 *  0 - Don't use TLS
 *  1 - Use TLS
 */
int
qdevice_net_check_tls_compatibility(enum tlv_tls_supported server_tls, enum tlv_tls_supported client_tls)
{
	int res;

	res = -1;

	switch (server_tls) {
	case TLV_TLS_UNSUPPORTED:
		switch (client_tls) {
		case TLV_TLS_UNSUPPORTED: res = 0; break ;
		case TLV_TLS_SUPPORTED: res = 0; break ;
		case TLV_TLS_REQUIRED: res = -1; break ;
		}
		break ;
	case TLV_TLS_SUPPORTED:
		switch (client_tls) {
		case TLV_TLS_UNSUPPORTED: res = 0; break ;
		case TLV_TLS_SUPPORTED: res = 1; break ;
		case TLV_TLS_REQUIRED: res = 1; break ;
		}
		break ;
	case TLV_TLS_REQUIRED:
		switch (client_tls) {
		case TLV_TLS_UNSUPPORTED: res = -1; break ;
		case TLV_TLS_SUPPORTED: res = 1; break ;
		case TLV_TLS_REQUIRED: res = 1; break ;
		}
		break ;
	}

	return (res);
}


int
qdevice_net_msg_received(struct qdevice_net_instance *instance)
{
	struct msg_decoded msg;
	int res;
	int ret_val;

	msg_decoded_init(&msg);

	res = msg_decode(&instance->receive_buffer, &msg);
	if (res != 0) {
		/*
		 * Error occurred. Disconnect.
		 */
		qdevice_net_log_msg_decode_error(res);
		qdevice_net_log(LOG_ERR, "Disconnecting from server");

		return (-1);
	}

	ret_val = 0;

	if (!msg.seq_number_set || msg.seq_number != instance->expected_msg_seq_num) {
		qdevice_net_log(LOG_ERR, "Received message doesn't contain seq_number or it's not expected one.");

		ret_val = -1;
		goto return_res;
	}

	switch (msg.type) {
	case MSG_TYPE_PREINIT:
		qdevice_net_log(LOG_ERR, "Received unexpected preinit message. Disconnecting from server");
		ret_val = -1;

		goto return_res;

		break;
	case MSG_TYPE_PREINIT_REPLY:
		if (instance->state != QDEVICE_NET_STATE_WAITING_PREINIT_REPLY) {
			qdevice_net_log(LOG_ERR, "Received unexpected preinit reply message. Disconnecting from server");

			ret_val = -1;

			goto return_res;
		}

		/*
		 * Check TLS support
		 */
		if (!msg.tls_supported_set || !msg.tls_client_cert_required_set) {
			qdevice_net_log(LOG_ERR, "Required tls_supported or tls_client_cert_required option is unset");

			ret_val = -1;

			goto return_res;
		}

		res = qdevice_net_check_tls_compatibility(msg.tls_supported, instance->tls_supported);
		if (res == -1) {
			qdevice_net_log(LOG_ERR, "Incompatible tls configuration (server %u client %u)",
			    msg.tls_supported, instance->tls_supported);

			ret_val = -1;

			goto return_res;
		} else if (res == 1) {
			/*
			 * Start TLS
			 */

		}

		break;
	default:
		qdevice_net_log(LOG_ERR, "Received unsupported message %u. Disconnecting from server", msg.type);
		ret_val = -1;
		goto return_res;

		break;
	}

return_res:
	msg_decoded_destroy(&msg);

	return (ret_val);
}

/*
 * -1 means end of connection (EOF) or some other unhandled error. 0 = success
 */
int
qdevice_net_socket_read(struct qdevice_net_instance *instance)
{
	int res;

	res = msgio_read(instance->socket, &instance->receive_buffer, &instance->msg_already_received_bytes,
	    &instance->skipping_msg);

	if (instance->skipping_msg) {
		qdevice_net_log(LOG_DEBUG, "msgio_read set skipping_msg");
	}

	if (res == -1) {
		qdevice_net_log(LOG_DEBUG, "Server closed connection");
		return (-1);
	}

	if (res == -2) {
		qdevice_net_log_nss(LOG_ERR, "Unhandled error when reading from server. Disconnecting from server");

		return (-1);
	}

	if (res == -3) {
		qdevice_net_log(LOG_ERR, "Can't store message header from server. Disconnecting from server");

		return (-1);
	}

	if (res == -4) {
		qdevice_net_log(LOG_ERR, "Can't store message from server. Disconnecting from server");

		return (-1);
	}

	if (res == -5) {
		qdevice_net_log(LOG_WARNING, "Server sent unsupported msg type %u. Disconnecting from server",
			    msg_get_type(&instance->receive_buffer));

		return (-1);
	}

	if (res == -6) {
		qdevice_net_log(LOG_WARNING,
		    "Server wants to send too long message %u bytes. Disconnecting from server",
		    msg_get_len(&instance->receive_buffer));

		return (-1);
	}

	if (res == 1) {
		/*
		 * Full message received / skipped
		 */
		if (!instance->skipping_msg) {
			fprintf(stderr, "FULL MESSAGE RECEIVED\n");
			if (qdevice_net_msg_received(instance) == -1) {
				return (-1);
			}
		} else {
			errx(1, "net_socket_read in skipping msg state");
		}

		instance->skipping_msg = 0;
	}

	return (0);
}

int
qdevice_net_socket_write(struct qdevice_net_instance *instance)
{
	int res;

	res = msgio_write(instance->socket, &instance->send_buffer, &instance->msg_already_sent_bytes);

	if (res == 1) {
		instance->sending_msg = 0;

	}

	if (res == -1) {
		qdevice_net_log_nss(LOG_CRIT, "PR_Send returned 0");

		return (-1);
	}

	if (res == -2) {
		qdevice_net_log_nss(LOG_ERR, "Unhandled error when sending message to server");

		return (-1);
	}

	return (0);
}

int
qdevice_net_schedule_send(struct qdevice_net_instance *instance)
{
	if (instance->sending_msg) {
		/*
		 * Msg is already scheduled for send
		 */
		return (-1);
	}

	instance->msg_already_sent_bytes = 0;
	instance->sending_msg = 1;

	return (0);
}

#define QDEVICE_NET_POLL_NO_FDS		1
#define QDEVICE_NET_POLL_SOCKET		0

int
qdevice_net_poll(struct qdevice_net_instance *instance)
{
	PRPollDesc pfds[QDEVICE_NET_POLL_NO_FDS];
	PRInt32 poll_res;
	int i;
	int schedule_disconnect;

	pfds[QDEVICE_NET_POLL_SOCKET].fd = instance->socket;
	pfds[QDEVICE_NET_POLL_SOCKET].in_flags = PR_POLL_READ;
	if (instance->sending_msg) {
		pfds[QDEVICE_NET_POLL_SOCKET].in_flags |= PR_POLL_WRITE;
	}

	schedule_disconnect = 0;

	if ((poll_res = PR_Poll(pfds, QDEVICE_NET_POLL_NO_FDS, PR_INTERVAL_NO_TIMEOUT)) > 0) {
		for (i = 0; i < QDEVICE_NET_POLL_NO_FDS; i++) {
			if (pfds[i].out_flags & PR_POLL_READ) {
				switch (i) {
				case QDEVICE_NET_POLL_SOCKET:
					if (qdevice_net_socket_read(instance) == -1) {
						schedule_disconnect = 1;
					}

					break ;
				default:
					errx(1, "Unhandled read poll descriptor %u", i);
					break ;
				}
			}

			if (!schedule_disconnect && pfds[i].out_flags & PR_POLL_WRITE) {
				switch (i) {
				case QDEVICE_NET_POLL_SOCKET:
					if (qdevice_net_socket_write(instance) == -1) {
						schedule_disconnect = 1;
					}

					break;
				default:
					errx(1, "Unhandled write poll descriptor %u", i);
					break;
				}
			}

			if (!schedule_disconnect &&
			    pfds[i].out_flags & (PR_POLL_ERR|PR_POLL_NVAL|PR_POLL_HUP|PR_POLL_EXCEPT)) {
				switch (i) {
				case QDEVICE_NET_POLL_SOCKET:
					qdevice_net_log(LOG_CRIT, "POLL_ERR (%u) on main socket", pfds[i].out_flags);

					return (-1);

					break;
				default:
					errx(1, "Unhandled poll err on descriptor %u", i);
					break;
				}
			}
		}
	}

	if (schedule_disconnect) {
		return (-1);
	}

	return (0);
}

int
qdevice_net_instance_init(struct qdevice_net_instance *instance, size_t max_receive_size,
    size_t max_send_size, enum tlv_tls_supported tls_supported)
{

	memset(instance, 0, sizeof(*instance));

	instance->max_receive_size = max_receive_size;
	instance->max_send_size = max_send_size;
	dynar_init(&instance->receive_buffer, max_receive_size);
	dynar_init(&instance->send_buffer, max_send_size);

	instance->tls_supported = tls_supported;

	return (0);
}

int
qdevice_net_instance_destroy(struct qdevice_net_instance *instance)
{

	dynar_destroy(&instance->receive_buffer);
	dynar_destroy(&instance->send_buffer);

	return (0);
}

int main(void)
{
	struct qdevice_net_instance instance;

	/*
	 * Init
	 */
	qdevice_net_log_init(QDEVICE_NET_LOG_TARGET_STDERR);
        qdevice_net_log_set_debug(1);

	if (nss_sock_init_nss(NSS_DB_DIR) != 0) {
		err_nss();
	}

	if (qdevice_net_instance_init(&instance, QDEVICE_NET_MAX_MSG_RECEIVE_SIZE,
	    QDEVICE_NET_MAX_MSG_SEND_SIZE, TLV_TLS_REQUIRED) == -1) {
		errx(1, "Can't initialize qdevice-net");
	}

	/*
	 * Try to connect to qnetd host
	 */
	instance.socket = nss_sock_create_client_socket(QNETD_HOST, QNETD_PORT, PR_AF_UNSPEC, 100);
	if (instance.socket == NULL) {
		err_nss();
	}

	if (nss_sock_set_nonblocking(instance.socket) != 0) {
		err_nss();
	}

	/*
	 * Create and schedule send of preinit message to qnetd
	 */
	instance.expected_msg_seq_num = 1;
	if (msg_create_preinit(&instance.send_buffer, "Cluster", 1, instance.expected_msg_seq_num) == 0) {
		errx(1, "Can't allocate buffer");
	}
	if (qdevice_net_schedule_send(&instance) != 0) {
		errx(1, "Can't schedule send of preinit msg");
	}

	instance.state = QDEVICE_NET_STATE_WAITING_PREINIT_REPLY;

	/*
	 * Main loop
	 */
	while (qdevice_net_poll(&instance) == 0) {
	}

	/*
	 * Cleanup
	 */
	if (PR_Close(instance.socket) != SECSuccess) {
		err_nss();
	}

	qdevice_net_instance_destroy(&instance);

	SSL_ClearSessionCache();

	if (NSS_Shutdown() != SECSuccess) {
		err_nss();
	}

	PR_Cleanup();

	qdevice_net_log_close();

	return (0);
}
