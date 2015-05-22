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

#define NSS_DB_DIR	"node/nssdb"

#define QNETD_HOST	"localhost"
#define QNETD_PORT	4433

#define QDEVICE_NET_MAX_MSG_RECEIVE_SIZE	(1 << 15)
#define QDEVICE_NET_MAX_MSG_SEND_SIZE		(1 << 15)

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
};

static void
err_nss(void) {
	errx(1, "nss error %d: %s", PR_GetError(), PR_ErrorToString(PR_GetError(), PR_LANGUAGE_I_DEFAULT));
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

int
qdevice_net_instance_init(struct qdevice_net_instance *instance, size_t max_receive_size,
    size_t max_send_size)
{

	memset(instance, 0, sizeof(*instance));

	instance->max_receive_size = max_receive_size;
	instance->max_send_size = max_send_size;
	dynar_init(&instance->receive_buffer, max_receive_size);
	dynar_init(&instance->send_buffer, max_send_size);

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
	size_t start_pos;
	ssize_t sent_bytes;

	if (nss_sock_init_nss(NSS_DB_DIR) != 0) {
		err_nss();
	}

	if (qdevice_net_instance_init(&instance, QDEVICE_NET_MAX_MSG_RECEIVE_SIZE,
	    QDEVICE_NET_MAX_MSG_SEND_SIZE) == -1) {
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
	if (msg_create_preinit(&instance.send_buffer, "Cluster", 1, 1) == 0) {
		errx(1, "Can't allocate buffer");
	}
	if (qdevice_net_schedule_send(&instance) != 0) {
		errx(1, "Can't schedule send of preinit msg");
	}

	start_pos = 0;
	sent_bytes = msgio_send_blocking(instance.socket, dynar_data(&instance.send_buffer),
	    dynar_size(&instance.send_buffer));
	if (sent_bytes == -1 || sent_bytes != dynar_size(&instance.send_buffer)) {
		err_nss();
	}

	if (PR_Close(instance.socket) != SECSuccess) {
		err_nss();
	}

	qdevice_net_instance_destroy(&instance);

	SSL_ClearSessionCache();

	if (NSS_Shutdown() != SECSuccess) {
		err_nss();
	}

	PR_Cleanup();

	return (0);
}
