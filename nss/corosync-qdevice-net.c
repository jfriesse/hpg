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

//#define ENABLE_TLS	1


static void err_nss(void) {
	errx(1, "nss error %d: %s", PR_GetError(), PR_ErrorToString(PR_GetError(), PR_LANGUAGE_I_DEFAULT));
}

/*static void warn_nss(void) {
	warnx("nss error %d: %s", PR_GetError(), PR_ErrorToString(PR_GetError(), PR_LANGUAGE_I_DEFAULT));
}*/

/*static SECStatus nss_bad_cert_hook(void *arg, PRFileDesc *fd) {
	if (PR_GetError() == SEC_ERROR_EXPIRED_CERTIFICATE || PR_GetError() == SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE ||
	    PR_GetError() == SEC_ERROR_CRL_EXPIRED || PR_GetError() == SEC_ERROR_KRL_EXPIRED ||
	    PR_GetError() == SSL_ERROR_EXPIRED_CERT_ALERT) {
		fprintf(stderr, "Expired certificate\n");
		return (SECSuccess);
	}

//	warn_nss();

	return (SECFailure);
}*/

/*static char *get_pwd(PK11SlotInfo *slot, PRBool retry, void *arg)
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
}*/

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

#ifdef ENABLE_TLS
				if (strcmp(to_send, "starttls\n") == 0) {
					if ((client_socket = nss_sock_start_ssl_as_client(client_socket, "Qnetd Server", nss_bad_cert_hook, NSS_GetClientAuthData, "Cluster Cert")) == NULL) {
						fprintf(stderr, "AAAAA\n");
						err_nss();
					}
				}
#endif
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

/*int send_preinit_msg(const char *cluster_name)
{
}*/

int main(void)
{
	PRFileDesc *qnet_socket;
	char msg[500012];
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
	 * Send preinit message to qnetd
	 */
	printf("Ahoj = %zu\n", msg_len = msg_create_preinit(msg, sizeof(msg), "ahoj", 1));

	size_t i;
	char c;

	for (i = 0; i < msg_len; i++) {
		c = msg[i];

/*		if (c < 32 || c > 126) {
			printf("x%02u", c);
		} else {
			printf("%c", c);
		}*/
		printf("%02x ", c);
	}
	printf("\n");
/*	handle_client(client_socket);*/
	start_pos = 0;
	sent_bytes = msgio_send_blocking(qnet_socket, msg, sizeof(msg));
	if (sent_bytes == -1 || sent_bytes != sizeof(msg)) {
		err_nss();
	}
	fprintf(stderr,"%zu %zu\n", sent_bytes, sizeof(msg));

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
