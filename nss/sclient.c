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

#include "nss-sock.h"

#define NSS_DB_DIR	"nssdb"

PRFileDesc *client_socket;

static void err_nss(void) {
	errx(1, "nss error %d: %s", PR_GetError(), PR_ErrorToString(PR_GetError(), PR_LANGUAGE_I_DEFAULT));
}

static char *get_pwd(PK11SlotInfo *slot, PRBool retry, void *arg)
{
	FILE *f;
	char pwd[255];

	if (retry) {
		return (NULL);
	}

	f = fopen(NSS_DB_DIR"/pwdfile.txt", "rt");
	if (f == NULL) {
		err(1, "Can't open pwd file");
	}

	fgets(pwd, sizeof(pwd), f);
	fclose(f);

	if (pwd[strlen(pwd) - 1] == '\n')
		pwd[strlen(pwd) - 1] = '\0';

	fprintf(stderr, "Return %s password\n", pwd);

	return (PL_strdup(pwd));
}

int
recv_from_server(PRFileDesc *socket)
{
	char buf[255];
	PRInt32 readed;

	fprintf(stderr, "PR_READ\n");
	readed = PR_Recv(socket, buf, sizeof(buf), 0, 1000);
	fprintf(stderr, "-PR_READ %u\n", readed);
	if (readed > 0) {
		buf[readed] = '\0';
		printf("Client %p readed %u bytes: %s\n", socket, readed, buf);
	}

	if (readed == 0) {
		printf("Client %p EOF\n", socket);
	}

	if (readed < 0 && PR_GetError() == PR_WOULD_BLOCK_ERROR) {
		fprintf(stderr, "WOULD BLOCK\n");
	}

	if (readed < 0 && PR_GetError() != PR_IO_TIMEOUT_ERROR && PR_GetError() != PR_WOULD_BLOCK_ERROR) {
		err_nss();
	}

	return (readed);
}

void
handle_client(PRFileDesc *socket)
{
	PRPollDesc pfds[2];
	PRInt32 res;
	int exit_loop;
	char to_send[255];
	PRInt32 sent;

	exit_loop = 0;

	while (!exit_loop) {
		fprintf(stderr,"Handle client loop\n");
		pfds[0].fd = PR_STDIN;
		pfds[0].in_flags = PR_POLL_READ | PR_POLL_EXCEPT;
		pfds[0].out_flags = 0;
		pfds[1].fd = socket;
		pfds[1].in_flags = PR_POLL_READ | PR_POLL_EXCEPT;
		pfds[1].out_flags = 0;

		if ((res = PR_Poll(pfds, 2, PR_INTERVAL_NO_TIMEOUT)) > 0) {
			if (pfds[0].out_flags & PR_POLL_READ) {
				if (fgets(to_send, sizeof(to_send), stdin) == NULL) {
					exit_loop = 1;
					break;
				}

				if ((sent = PR_Send(socket, to_send, strlen(to_send), 0, PR_INTERVAL_NO_TIMEOUT)) == -1) {
					err_nss();
				}
				fprintf(stderr,"sent = %u\n", sent);
			}

			if (pfds[1].out_flags & PR_POLL_READ) {
				if (recv_from_server(pfds[1].fd) == 0) {
					exit_loop = 1;
				}
			}

			if (pfds[1].out_flags & PR_POLL_ERR) {
				fprintf(stderr, "ERR\n");
			}

			if (pfds[1].out_flags & PR_POLL_NVAL) {
				fprintf(stderr, "NVAL\n");
			}

			if (pfds[1].out_flags & PR_POLL_HUP) {
				fprintf(stderr, "HUP\n");
			}

			if (pfds[1].out_flags & PR_POLL_EXCEPT) {
				fprintf(stderr, "EXCEPT\n");
			}
		}
	}
}

static SECStatus nss_bad_cert_hook(void *arg, PRFileDesc *fd) {
	err_nss();
	return SECSuccess;
}

int main(void)
{

	if (nss_sock_init_nss(NSS_DB_DIR) != 0) {
		err_nss();
	}

	PK11_SetPasswordFunc(get_pwd);

	client_socket = nss_sock_create_client_socket("localhost", 4433, PR_AF_UNSPEC, 100);
	if (client_socket == NULL) {
		err_nss();
	}

	client_socket = SSL_ImportFD(NULL, client_socket);

	if (SSL_SetURL(client_socket, "Qnetd Server") != SECSuccess) {
		err_nss();
	}

	if ((SSL_OptionSet(client_socket, SSL_SECURITY, PR_TRUE) != SECSuccess) ||
	    (SSL_OptionSet(client_socket, SSL_HANDSHAKE_AS_SERVER, PR_FALSE) != SECSuccess) ||
	    (SSL_OptionSet(client_socket, SSL_HANDSHAKE_AS_CLIENT, PR_TRUE) != SECSuccess) ||
	    (SSL_AuthCertificateHook(client_socket, SSL_AuthCertificate, CERT_GetDefaultCertDB()) != SECSuccess) ||
	    (SSL_BadCertHook(client_socket, nss_bad_cert_hook, NULL) != SECSuccess)) {
		err_nss();
	}

	if (SSL_ResetHandshake(client_socket, PR_FALSE) != SECSuccess) {
		err_nss();
	}

	if (SSL_ForceHandshake(client_socket) != SECSuccess) {
		err_nss();
	}

	handle_client(client_socket);

	if (PR_Close(client_socket) != SECSuccess) {
		err_nss();
	}

	SSL_ClearSessionCache();

	if (NSS_Shutdown() != SECSuccess) {
		err_nss();
	}

	PR_Cleanup();

	return (0);
}
