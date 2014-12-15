#include <stdio.h>
#include <nss.h>
#include <ssl.h>
#include <prio.h>
#include <prnetdb.h>
#include <prerror.h>
#include <prinit.h>
#include <getopt.h>
#include <err.h>

#define NSS_DB_DIR	"nssdb"

static void err_nss(void) {
	errx(1, "nss error %d: %s", PR_GetError(), PR_ErrorToString(PR_GetError(), PR_LANGUAGE_I_DEFAULT));
}

int main(void)
{
	PRFileDesc *tmp_sock, *socket;
	PRAddrInfo *addr_info;
	void *addr_iter;
	PRNetAddr addr, client_addr;
	int addr_bound;
	PRFileDesc* client_socket;

	if (NSS_Init(NSS_DB_DIR) != SECSuccess) {
		err_nss();
	}

	tmp_sock = PR_OpenTCPSocket(PR_AF_INET);
	if (tmp_sock == NULL) {
		err_nss();
	}

	socket = SSL_ImportFD(NULL, tmp_sock);
	if (socket == NULL) {
		err_nss();
	}

	if ((SSL_OptionSet(socket, SSL_SECURITY, PR_TRUE) != SECSuccess) ||
	    (SSL_OptionSet(socket, SSL_HANDSHAKE_AS_SERVER, PR_TRUE) != SECSuccess) ||
	    (SSL_OptionSet(socket, SSL_HANDSHAKE_AS_CLIENT, PR_FALSE) != SECSuccess)) {
		err_nss();
	}

	/*
	 * Socket is ready, now find addr to bind to
	 */
	addr_info = PR_GetAddrInfoByName("127.0.0.1", PR_AF_INET, PR_AI_ADDRCONFIG);
	if (addr_info == NULL) {
		err_nss();
	}

	addr_iter = NULL;
	addr_bound = 0;

	while ((addr_iter = PR_EnumerateAddrInfo(addr_iter, addr_info, 4433, &addr)) != NULL) {
		if (PR_Bind(socket, &addr) == PR_SUCCESS) {
			addr_bound = 1;
			break;
		}
	}

	if (!addr_bound) {
		errx(1, "Can't bound to address");
	}

	if (PR_Listen(socket, 10) != PR_SUCCESS) {
		err_nss();
	}

	if ((client_socket = PR_Accept(socket, &client_addr, PR_INTERVAL_NO_TIMEOUT)) == NULL) {
		err_nss();
	}

	client_socket = SSL_ImportFD(NULL, client_socket);
	if (client_socket == NULL) {
		err_nss();
	}

	if ((SSL_OptionSet(client_socket, SSL_SECURITY, PR_TRUE) != SECSuccess) ||
	    (SSL_OptionSet(client_socket, SSL_HANDSHAKE_AS_SERVER, PR_TRUE) != SECSuccess) ||
	    (SSL_OptionSet(client_socket, SSL_HANDSHAKE_AS_CLIENT, PR_FALSE) != SECSuccess)) {
		err_nss();
	}

	if (SSL_ForceHandshake(client_socket) != SECSuccess) {
		err_nss();
	}

	PR_Close(socket);

	return (0);
}
