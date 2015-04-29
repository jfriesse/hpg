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
#include "qnetd-client.h"
#include "qnetd-clients-list.h"
#include "qnetd-poll-array.h"

#define QNETD_HOST      NULL
#define QNETD_PORT      4433

#define NSS_DB_DIR	"nssdb"
#define QNETD_CERT_NICKNAME	"QNetd Cert"


struct qnetd_instance {
	struct {
		PRFileDesc *socket;
		CERTCertificate *cert;
		SECKEYPrivateKey *private_key;
	} server;
	struct qnetd_clients_list clients;
};

static void err_nss(void) {
	errx(1, "nss error %d: %s", PR_GetError(), PR_ErrorToString(PR_GetError(), PR_LANGUAGE_I_DEFAULT));
}

PRFileDesc *
accept_connection(const struct qnetd_instance *instance)
{
	PRNetAddr client_addr;
	PRFileDesc *client_socket;

	if ((client_socket = PR_Accept(instance->server.socket, &client_addr, PR_INTERVAL_NO_TIMEOUT)) == NULL) {
		err_nss();
	}

	return (client_socket);
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

int main(void)
{
	struct qnetd_clients_list clients;
	struct qnetd_client *cl;
	int i;
	void *p;
	PRPollDesc *pd;
	struct qnetd_poll_array pa;

	qnetd_clients_list_init(&clients);

	for (i = 0; i < 10; i++) {
		p = (void *)(uint64_t)i;
		cl = qnetd_clients_list_add(&clients, p);
	}

	TAILQ_FOREACH(cl, &clients, entries) {
		fprintf(stderr, "%u\n", (uint32_t)((uint64_t)cl->socket));
	}

	cl = TAILQ_NEXT(TAILQ_FIRST(&clients), entries);
	qnetd_clients_list_del(&clients, cl);

	TAILQ_FOREACH(cl, &clients, entries) {
		fprintf(stderr, "%u\n", (uint32_t)((uint64_t)cl->socket));
	}

	qnetd_poll_array_init(&pa);


	pd = qnetd_poll_array_create_from_clients_list(&pa, &clients, (void *)(uint64_t)66, 0);
	fprintf(stderr, "%u\n", qnetd_poll_array_size(&pa));

	for (i = 0; i < qnetd_poll_array_size(&pa); i++) {
		fprintf(stderr, "%u %u %u\n", i, (uint32_t)((uint64_t)pd[i].fd), pd[i].in_flags);
	}

	//cl = TAILQ_NEXT(TAILQ_FIRST(&clients), entries);
	//qnetd_clients_list_del(&clients, cl);
	qnetd_clients_list_free(&clients);

	pd = qnetd_poll_array_create_from_clients_list(&pa, &clients, (void *)(uint64_t)66, 0);
	fprintf(stderr, "%u\n", qnetd_poll_array_size(&pa));

	for (i = 0; i < qnetd_poll_array_size(&pa); i++) {
		fprintf(stderr, "%u %u %u\n", i, (uint32_t)((uint64_t)pd[i].fd), pd[i].in_flags);
	}

	qnetd_poll_array_destroy(&pa);
	/*struct qnetd_poll_array pa;
	int i;
	PRPollDesc *prpd;

	qnetd_poll_array_init(&pa);

	fprintf(stderr, "%u %u %u\n", qnetd_poll_array_combined_size(&pa), qnetd_poll_array_size(&pa),
	    qnetd_poll_array_special_size(&pa));

	for (i = 0; i < 10; i++) {
		prpd = qnetd_poll_array_add(&pa);
		prpd->in_flags = i;
	}

	for (i = 0; i < qnetd_poll_array_size(&pa); i++) {
		fprintf(stderr, "%u\n", qnetd_poll_array_get(&pa, i)->in_flags);
	}

	fprintf(stderr, "%u %u %u\n", qnetd_poll_array_combined_size(&pa), qnetd_poll_array_size(&pa),
	    qnetd_poll_array_special_size(&pa));*/

	exit(1);

	struct qnetd_instance instance;

	if (nss_sock_init_nss(NSS_DB_DIR) != 0) {
		err_nss();
	}

	if (SSL_ConfigServerSessionIDCache(0, 0, 0, NULL) != SECSuccess) {
		err_nss();
	}

	if (qnetd_instance_init_certs(&instance) == -1) {
		err_nss();
	}

	instance.server.socket = nss_sock_create_listen_socket(QNETD_HOST, QNETD_PORT, PR_AF_INET6);
	if (instance.server.socket == NULL) {
		err_nss();
	}

	if (nss_sock_set_nonblocking(instance.server.socket) != 0) {
		err_nss();
	}

	if (PR_Listen(instance.server.socket, 10) != PR_SUCCESS) {
		err_nss();
	}

	PR_Close(instance.server.socket);
	CERT_DestroyCertificate(instance.server.cert);
	SECKEY_DestroyPrivateKey(instance.server.private_key);

	SSL_ClearSessionCache();

	SSL_ShutdownServerSessionIDCache();

	if (NSS_Shutdown() != SECSuccess) {
		err_nss();
	}

	PR_Cleanup();

	return (0);
}
