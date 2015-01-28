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

/*void
accept_connection(void)
{
	PRNetAddr client_addr;
	PRFileDesc *client_socket;
	struct client_item *ci;

	if ((client_socket = PR_Accept(server.socket, &client_addr, PR_INTERVAL_NO_TIMEOUT)) == NULL) {
		err_nss();
	}

	if (nss_sock_set_nonblocking(client_socket) != 0) {
		err_nss();
	}

	ci = calloc(1, sizeof(*ci));
	client_socket = SSL_ImportFD(NULL, client_socket);
	if (client_socket == NULL) {
		err_nss();
	}

	if (SSL_ConfigSecureServer(client_socket,
	    server.cert,
	    server.private_key,
	    NSS_FindCertKEAType(server.cert)) != PR_SUCCESS) {
		err_nss();
	}

	if ((SSL_OptionSet(client_socket, SSL_SECURITY, PR_TRUE) != SECSuccess) ||
	    (SSL_OptionSet(client_socket, SSL_HANDSHAKE_AS_SERVER, PR_TRUE) != SECSuccess) ||
	    (SSL_OptionSet(client_socket, SSL_HANDSHAKE_AS_CLIENT, PR_FALSE) != SECSuccess)) {
		err_nss();
	}

	if (SSL_ResetHandshake(client_socket, PR_TRUE) != SECSuccess) {
		err_nss();
	}

	ci->socket = client_socket;
	PR_APPEND_LINK(&ci->list, &clients);
}

int
recv_from_client(PRFileDesc *socket)
{
	char buf[255];
	PRInt32 readed;

	fprintf(stderr, "PR_READ\n");
	readed = PR_Recv(socket, buf, sizeof(buf), 0, 0);
	fprintf(stderr, "-PR_READ\n");
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

int
no_clients(void)
{
	struct client_item *iter;
	int res;

	res = 0;

	for (iter = (struct client_item *)PR_LIST_HEAD(&clients);
	    (void *)iter != (void *)&clients;
	    iter = (struct client_item *)PR_NEXT_LINK(&iter->list)) {
		res++;
	}

	return (res);
}

void
main_loop(void)
{
	PRPollDesc *pfds;
	PRInt32 res;
	struct client_item *iter;
	int i, j;
	int no_items;

	no_items = no_clients() + 1;

	pfds = malloc(sizeof(*pfds) * no_items);

	pfds[0].fd = server.socket;
	pfds[0].in_flags = PR_POLL_READ | PR_POLL_EXCEPT;
	pfds[0].out_flags = 0;

	for (iter = (struct client_item *)PR_LIST_HEAD(&clients), i = 1;
	    (void *)iter != (void *)&clients;
	    iter = (struct client_item *)PR_NEXT_LINK(&iter->list), i++) {
		pfds[i].fd = iter->socket;
		pfds[i].in_flags = PR_POLL_READ | PR_POLL_EXCEPT;
		pfds[i].out_flags = 0;
	}

	if ((res = PR_Poll(pfds, no_items, PR_INTERVAL_NO_TIMEOUT)) > 0) {
		for (i = 0; i < no_items; i++) {
			if (pfds[i].out_flags & PR_POLL_READ) {
				fprintf(stderr, "Read %u\n", i);
				if (i == 0) {
					accept_connection();
				} else {
					if (recv_from_client(pfds[i].fd) == 0) {
						for (iter = (struct client_item *)PR_LIST_HEAD(&clients), j = 1;
						    j == i;
						    iter = (struct client_item *)PR_NEXT_LINK(&iter->list), j++) {
							PR_REMOVE_AND_INIT_LINK(&iter->list);
							free(iter);

							break ;
						}
					}
				}
			}

			if (pfds[i].out_flags & PR_POLL_ERR) {
				fprintf(stderr, "ERR\n");
			}

			if (pfds[i].out_flags & PR_POLL_NVAL) {
				fprintf(stderr, "NVAL\n");
			}

			if (pfds[i].out_flags & PR_POLL_HUP) {
				fprintf(stderr, "HUP\n");
			}

			if (pfds[i].out_flags & PR_POLL_EXCEPT) {
				fprintf(stderr, "EXCEPT\n");
			}
		}
	}

	free(pfds);
}
*/

static SECStatus nss_bad_cert_hook(void *arg, PRFileDesc *fd) {
	err_nss();
	return SECSuccess;
}

int main(void)
{
	char buf[255];
	PRInt32 readed;

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


	fprintf(stderr, "PR_READ\n");
	readed = PR_Recv(client_socket, buf, sizeof(buf), 0, PR_INTERVAL_NO_TIMEOUT);
	if (readed > 0) {
		buf[readed] = '\0';
		printf("Server %p readed %u bytes: %s\n", socket, readed, buf);
	}
	fprintf(stderr, "-PR_READ\n");

/*	if (nss_sock_set_nonblocking(server.socket) != 0) {
		err_nss();
	}

	if (PR_Listen(server.socket, 10) != PR_SUCCESS) {
		err_nss();
	}

	while (1) {
		main_loop();
	}*/

	PR_Close(client_socket);
	/*CERT_DestroyCertificate(server.cert);
	SECKEY_DestroyPrivateKey(server.private_key);*/

	if (NSS_Shutdown() != SECSuccess) {
		err_nss();
	}

	PR_Cleanup();

	return (0);
}
