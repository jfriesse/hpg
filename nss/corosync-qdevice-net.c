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

enum msg_type {
	MSG_TYPE_PREINIT = 0,
	MSG_TYPE_PREINIT_REPLY = 1,
};

enum tlv_opt_type {
	TLV_OPT_MSG_SEQ_NUMBER = 0,
	TLV_OPT_CLUSTER_NAME = 1,
};

#define MSG_TYPE_LENGTH		2
#define MSG_LENGTH_LENGTH	4

void
msg_set_type(char *msg, enum msg_type type)
{
	uint16_t ntype;

	ntype = htons((uint16_t)type);
	memcpy(msg, &ntype, sizeof(ntype));
}

void
msg_set_len(char *msg, uint32_t len)
{
	uint32_t nlen;

	nlen = htonl(len);
	memcpy(msg + 2, &nlen, sizeof(nlen));
}

int
tlv_add(char *msg, size_t msg_len, size_t *pos, enum tlv_opt_type opt_type, uint16_t opt_len,
	const void *value)
{
	uint16_t nlen;
	uint16_t nopt_type;

	if (*pos + sizeof(nopt_type) + sizeof(nlen) + opt_len > msg_len) {
		return (-1);
	}

	nopt_type = htons((uint16_t)opt_type);
	memcpy(msg + *pos, &nopt_type, sizeof(nopt_type));
	*pos += sizeof(nopt_type);

	nlen = htons(opt_len);
	memcpy(msg + *pos, &opt_len, sizeof(opt_len));
	*pos += sizeof(opt_len);

	memcpy(msg + *pos, value, opt_len);
	*pos += opt_len;

	return (0);
}

int
tlv_add_u32(char *msg, size_t msg_len, size_t *pos, enum tlv_opt_type opt_type, uint32_t u32)
{
	uint32_t nu32;

	nu32 = htonl(u32);

	return (tlv_add(msg, msg_len, pos, opt_type, sizeof(nu32), &nu32));
}

int
tlv_add_string(char *msg, size_t msg_len, size_t *pos, enum tlv_opt_type opt_type, const char *str)
{
	return (tlv_add(msg, msg_len, pos, opt_type, strlen(str), str));
}

int
tlv_add_msg_seq_number(char *msg, size_t msg_len, size_t *pos, uint32_t msg_seq_number)
{
	return (tlv_add_u32(msg, msg_len, pos, TLV_OPT_MSG_SEQ_NUMBER, msg_seq_number));
}

int
tlv_add_cluster_name(char *msg, size_t msg_len, size_t *pos, const char *cluster_name)
{
	return (tlv_add_string(msg, msg_len, pos, TLV_OPT_CLUSTER_NAME, cluster_name));
}

size_t
msg_create_preinit(char *msg, size_t msg_len, const char *cluster_name)
{
	size_t pos;

	msg_set_type(msg, MSG_TYPE_PREINIT);

	pos = MSG_TYPE_LENGTH + MSG_LENGTH_LENGTH;

	if (tlv_add_msg_seq_number(msg, msg_len, &pos, 1) == -1) {
		goto small_buf_err;
	}

	if (tlv_add_cluster_name(msg, msg_len, &pos, cluster_name) == -1) {
		goto small_buf_err;
	}

	msg_set_len(msg, pos - (MSG_TYPE_LENGTH + MSG_LENGTH_LENGTH));

	return (pos);

small_buf_err:
	return (0);
}

/*int send_preinit_msg(const char *cluster_name)
{
}*/

int main(void)
{
	PRFileDesc *qnet_socket;
	char msg[512];
	size_t msg_len;

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

	/*
	 * Send preinit message to qnetd
	 */
	printf("Ahoj = %zu\n", msg_len = msg_create_preinit(msg, sizeof(msg), "ahoj"));

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
