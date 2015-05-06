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
#include <syslog.h>

#include "nss-sock.h"
#include "qnetd-client.h"
#include "qnetd-clients-list.h"
#include "qnetd-poll-array.h"
#include "qnetd-log.h"
#include "dynar.h"

#define QNETD_HOST      NULL
#define QNETD_PORT      4433
#define QNETD_LISTEN_BACKLOG	10

#define NSS_DB_DIR	"nssdb"
#define QNETD_CERT_NICKNAME	"QNetd Cert"

struct qnetd_instance {
	struct {
		PRFileDesc *socket;
		CERTCertificate *cert;
		SECKEYPrivateKey *private_key;
	} server;
	struct qnetd_clients_list clients;
	struct qnetd_poll_array poll_array;
};



static void qnetd_err_nss(void) {
	qnetd_log_nss(LOG_CRIT, "NSS error");

	exit(1);
}

void
qnetd_client_net_read(struct qnetd_client *client)
{
	fprintf(stderr, "Client message received\n");
}

int
qnetd_client_accept(struct qnetd_instance *instance)
{
	PRNetAddr client_addr;
	PRFileDesc *client_socket;
	struct qnetd_client *client;

        if ((client_socket = PR_Accept(instance->server.socket, &client_addr, PR_INTERVAL_NO_TIMEOUT)) == NULL) {
		qnetd_log_nss(LOG_ERR, "Can't accept connection");
		return (-1);
	}

	if (nss_sock_set_nonblocking(client_socket) != 0) {
		qnetd_log_nss(LOG_ERR, "Can't set client socket to non blocking mode");
		return (-1);
	}

	client = qnetd_clients_list_add(&instance->clients, client_socket, &client_addr);
	if (client == NULL) {
		qnetd_log(LOG_ERR, "Can't add client to list");
		return (-2);
	}

	return (0);
}

int
qnetd_poll(struct qnetd_instance *instance)
{
	struct qnetd_client *client;
	PRPollDesc *pfds;
	PRInt32 poll_res;
	int i;

	client = NULL;

	pfds = qnetd_poll_array_create_from_clients_list(&instance->poll_array,
	    &instance->clients, instance->server.socket, PR_POLL_READ);

	if (pfds == NULL) {
		return (-1);
	}

	if ((poll_res = PR_Poll(pfds, qnetd_poll_array_size(&instance->poll_array),
	    PR_INTERVAL_NO_TIMEOUT)) > 0) {
		/*
		 * Walk thru pfds array and process events
		 */
		for (i = 0; i < qnetd_poll_array_size(&instance->poll_array); i++) {
			/*
			 * Also traverse clients list
			 */
			if (i > 0) {
				if (i == 1) {
					client = TAILQ_FIRST(&instance->clients);
				} else {
					client = TAILQ_NEXT(client, entries);
				}
			}

			if (pfds[i].out_flags & PR_POLL_READ) {
				if (i == 0) {
					qnetd_client_accept(instance);
				} else {
					qnetd_client_net_read(client);
				}
			}

			if (pfds[i].out_flags & (PR_POLL_ERR|PR_POLL_NVAL|PR_POLL_HUP|PR_POLL_EXCEPT)) {
				if (i == 0) {
					/*
					 * Poll ERR on listening socket is fatal error
					 */
					qnetd_log(LOG_CRIT, "POLL_ERR (%u) on listening socket", pfds[i].out_flags);
					return (-1);

				} else {
//					qnetd_client_poll_err(client);
				}
			}
		}
	}

	return (0);
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

int
qnetd_instance_init(struct qnetd_instance *instance)
{

	memset(instance, 0, sizeof(*instance));
	qnetd_poll_array_init(&instance->poll_array);
	qnetd_clients_list_init(&instance->clients);

	return (0);
}


int main(void)
{
	struct qnetd_instance instance;

	qnetd_log_init(QNETD_LOG_TARGET_STDERR);

	if (nss_sock_init_nss(NSS_DB_DIR) != 0) {
		qnetd_err_nss();
	}

	if (SSL_ConfigServerSessionIDCache(0, 0, 0, NULL) != SECSuccess) {
		qnetd_err_nss();
	}

	if (qnetd_instance_init(&instance) == -1) {
		errx(1, "Can't initialize qnetd");
	}

	if (qnetd_instance_init_certs(&instance) == -1) {
		qnetd_err_nss();
	}

	instance.server.socket = nss_sock_create_listen_socket(QNETD_HOST, QNETD_PORT, PR_AF_INET6);
	if (instance.server.socket == NULL) {
		qnetd_err_nss();
	}

	if (nss_sock_set_nonblocking(instance.server.socket) != 0) {
		qnetd_err_nss();
	}

	if (PR_Listen(instance.server.socket, QNETD_LISTEN_BACKLOG) != PR_SUCCESS) {
		qnetd_err_nss();
	}

	while (1) {
		qnetd_poll(&instance);
		fprintf(stderr,"POLL\n");
	}

	PR_Close(instance.server.socket);
	CERT_DestroyCertificate(instance.server.cert);
	SECKEY_DestroyPrivateKey(instance.server.private_key);

	SSL_ClearSessionCache();

	SSL_ShutdownServerSessionIDCache();

	if (NSS_Shutdown() != SECSuccess) {
		qnetd_err_nss();
	}

	PR_Cleanup();

	qnetd_log_close();

	return (0);
}
