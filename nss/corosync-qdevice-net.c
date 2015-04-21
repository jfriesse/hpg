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

#include "nss-sock.h"
#include "tlv.h"
#include "msg.h"
#include "msgio.h"

#define NSS_DB_DIR	"node/nssdb"

#define QNETD_HOST	"localhost"
#define QNETD_PORT	4433

static void err_nss(void) {
	errx(1, "nss error %d: %s", PR_GetError(), PR_ErrorToString(PR_GetError(), PR_LANGUAGE_I_DEFAULT));
}

int main(void)
{
	PRFileDesc *qnet_socket;
	char msg[65535];
	size_t msg_len;
	size_t start_pos;
	ssize_t sent_bytes;

	if (nss_sock_init_nss(NSS_DB_DIR) != 0) {
		err_nss();
	}

	/*
	 * Try to connect to qnetd host
	 */
	qnet_socket = nss_sock_create_client_socket(QNETD_HOST, QNETD_PORT, PR_AF_UNSPEC, 100);
	if (qnet_socket == NULL) {
		err_nss();
	}

	if (nss_sock_set_nonblocking(qnet_socket) != 0) {
		err_nss();
	}

	/*
	 * Create and send preinit message to qnetd
	 */
	msg_len = msg_create_preinit(msg, sizeof(msg), "Cluster", 1);
	if (msg_len == 0) {
		errx(1, "Can't allocate buffer");
	}

	start_pos = 0;
	sent_bytes = msgio_send_blocking(qnet_socket, msg, sizeof(msg));
	if (sent_bytes == -1 || sent_bytes != sizeof(msg)) {
		err_nss();
	}

	if (PR_Close(qnet_socket) != SECSuccess) {
		err_nss();
	}

	SSL_ClearSessionCache();

	if (NSS_Shutdown() != SECSuccess) {
		err_nss();
	}

	PR_Cleanup();

	return (0);
}
