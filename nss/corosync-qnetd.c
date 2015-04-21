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

#define QNETD_HOST      NULL
#define QNETD_PORT      4433

#define NSS_DB_DIR	"nssdb"
#define QNETD_CERT_NICKNAME	"QNetd Cert"

struct qnetd_client {
	PRFileDesc *socket;
	unsigned int test;
};

struct qnetd_clients_array {
	struct qnetd_client *array;
	unsigned int items;
	unsigned int allocated;
};

struct qnetd_instance {
	struct {
		PRFileDesc *socket;
		CERTCertificate *cert;
		SECKEYPrivateKey *private_key;
	} server;
	struct qnetd_clients_array clients;
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


void
qnetd_clients_array_init(struct qnetd_clients_array *clients_array)
{

	memset(clients_array, 0, sizeof(*clients_array));
}

struct qnetd_client *
qnetd_clients_array_add(struct qnetd_clients_array *clients_array)
{
	unsigned int new_clients_array_allocated;
	struct qnetd_client *new_clients_array;

	if (clients_array->items >= clients_array->allocated) {
		new_clients_array_allocated = (clients_array->allocated * 2) + 1;

		new_clients_array = realloc(clients_array->array,
		     sizeof(struct qnetd_client) * new_clients_array_allocated);

		if (new_clients_array == NULL) {
			return (NULL);
		}

		clients_array->allocated = new_clients_array_allocated;
		clients_array->array = new_clients_array;
	}

	clients_array->items++;

	return (&clients_array->array[clients_array->items - 1]);
}


struct qnetd_client *
qnetd_clients_array_get(const struct qnetd_clients_array *clients_array, unsigned int pos)
{

	if (pos >= clients_array->items) {
		return (NULL);
	}

	return (&clients_array->array[pos]);
}

unsigned int
qnetd_clients_array_size(const struct qnetd_clients_array *clients_array)
{

	return (clients_array->items);
}

void
qnetd_clients_array_del(struct qnetd_clients_array *clients_array, const struct qnetd_client *item)
{
	void *last_item_end_ptr;
	void *src_ptr;
	void *dst_ptr;

	last_item_end_ptr = (void *)clients_array->array + (sizeof(*item) * clients_array->items);
	src_ptr = (void *)item + sizeof(*item);
	dst_ptr = (void *)item;

	memmove(dst_ptr, src_ptr, last_item_end_ptr - src_ptr);

	clients_array->items--;
}


int main(void)
{
	struct qnetd_instance instance;
	struct qnetd_clients_array cla;
	struct qnetd_client *cl1, *cl2, *cl3, *cl4;
	int i;

	qnetd_clients_array_init(&cla);


/*	cl1 = qnetd_clients_array_add(&cla);
	cl1->test = 1;
	cl2 = qnetd_clients_array_add(&cla);
	cl2->test = 2;*/
/*	cl3 = qnetd_clients_array_add(&cla);
	cl3->test = 3;
	cl4 = qnetd_clients_array_add(&cla);
	cl4->test = 4;*/

/*	cl1 = qnetd_clients_array_get(&cla, 0);
	cl2 = qnetd_clients_array_get(&cla, 1);

	qnetd_clients_array_del(&cla, cl2);*/

	for (i = 0; i < 200; i++) {
		cl1 = qnetd_clients_array_add(&cla);
		cl1->test = i;
	}

	qnetd_clients_array_del(&cla, qnetd_clients_array_get(&cla, 100));

	for (i = 0; i < qnetd_clients_array_size(&cla); i++) {
		printf("%u = %u\n", i, qnetd_clients_array_get(&cla, i)->test);
	}

	for (i = 0; qnetd_clients_array_size(&cla) > 10; i++) {
		qnetd_clients_array_del(&cla, qnetd_clients_array_get(&cla, random() % qnetd_clients_array_size(&cla) + 1));
	}

	for (i = 0; i < qnetd_clients_array_size(&cla); i++) {
		printf("%u = %u\n", i, qnetd_clients_array_get(&cla, i)->test);
	}

/*	cl2 = qnetd_clients_array_add(&cla);
	cl2->socket = (void*)2;*/

	exit(1);
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