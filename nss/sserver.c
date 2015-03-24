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

#define ENABLE_TLS	1

struct server_item {
	PRFileDesc *socket;
	CERTCertificate *cert;
	SECKEYPrivateKey *private_key;
};

struct server_item server;

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

/*static SECStatus nss_bad_cert_hook(void *arg, PRFileDesc *fd) {
    err_nss();

    return SECFailure;
}*/

PRFileDesc *
accept_connection(void)
{
	PRNetAddr client_addr;
	PRFileDesc *client_socket;

	if ((client_socket = PR_Accept(server.socket, &client_addr, PR_INTERVAL_NO_TIMEOUT)) == NULL) {
		err_nss();
	}

	return (client_socket);
}

int
recv_from_client(PRFileDesc **socket)
{
	char buf[255];
	PRInt32 readed;

	fprintf(stderr, "PR_READ\n");
	readed = PR_Recv(*socket, buf, sizeof(buf), 0, 0);
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
/*	if (readed < 0) {
		err_nss();
	}*/

#ifdef ENABLE_TLS
	if (strcmp(buf, "starttls\n") == 0) {
		*socket = nss_sock_start_ssl_as_server(*socket, server.cert, server.private_key, PR_TRUE);
		if (*socket == NULL) {
			fprintf(stderr, "AAAA\n");
			err_nss();
		}
	}
#endif

	return (readed);
}

void
handle_client(PRFileDesc **socket)
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
		pfds[1].fd = *socket;
		pfds[1].in_flags = PR_POLL_READ | PR_POLL_EXCEPT;
		pfds[1].out_flags = 0;

		if ((res = PR_Poll(pfds, 2, PR_INTERVAL_NO_TIMEOUT)) > 0) {
			if (pfds[0].out_flags & PR_POLL_READ) {
				fgets(to_send, sizeof(to_send), stdin);
				if ((sent = PR_Send(*socket, to_send, strlen(to_send), 0, PR_INTERVAL_NO_TIMEOUT)) == -1) {
					err_nss();
				}
				fprintf(stderr,"sent = %u\n", sent);
			}

			if (pfds[1].out_flags & PR_POLL_READ) {
				if (recv_from_client(socket) == 0) {
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

int main(void)
{
	PRFileDesc *client_socket;

	if (nss_sock_init_nss(NSS_DB_DIR) != 0) {
		err_nss();
	}

	if (SSL_ConfigServerSessionIDCache (0, 0, 0, NULL) != SECSuccess) {
		err_nss();
	}

	PK11_SetPasswordFunc(get_pwd);

	server.cert =  PK11_FindCertFromNickname("QNetd Cert", NULL);
	if (server.cert == NULL) {
		err_nss();
	}

	server.private_key = PK11_FindKeyByAnyCert(server.cert, NULL);
	if (server.private_key == NULL) {
		err_nss();
	}

	server.socket = nss_sock_create_listen_socket(NULL, 4433, PR_AF_INET6);
	if (server.socket == NULL) {
		err_nss();
	}

	if (PR_Listen(server.socket, 10) != PR_SUCCESS) {
		err_nss();
	}

/*	while (1) {*/
		fprintf(stderr,"Accept connection\n");
		client_socket = accept_connection();

#ifndef ENABLE_TLS
		client_socket = nss_sock_start_ssl_as_server(client_socket, server.cert, server.private_key, PR_TRUE);
		if (*socket == NULL) {
			fprintf(stderr, "AAAA\n");
			err_nss();
		}
#endif

		handle_client(&client_socket);
		PR_Close(client_socket);
/*	}*/

	PR_Close(server.socket);
	CERT_DestroyCertificate(server.cert);
	SECKEY_DestroyPrivateKey(server.private_key);

/*	SSL_ClearSessionCache();*/

	SSL_ShutdownServerSessionIDCache();

	if (NSS_Shutdown() != SECSuccess) {
		err_nss();
	}

	PR_Cleanup();

	return (0);
}
