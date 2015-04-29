#include <sys/types.h>
#include <arpa/inet.h>

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "qnetd-clients-list.h"

void
qnetd_clients_list_init(struct qnetd_clients_list *clients_list)
{

	TAILQ_INIT(clients_list);
}

struct qnetd_client *
qnetd_clients_list_add(struct qnetd_clients_list *clients_list, PRFileDesc *socket)
{
	struct qnetd_client *client;

	client = (struct qnetd_client *)malloc(sizeof(*client));
	if (client == NULL) {
		return (NULL);
	}

	qnetd_client_init(client, socket);

	TAILQ_INSERT_TAIL(clients_list, client, entries);

	return (client);
}

void
qnetd_clients_list_free(struct qnetd_clients_list *clients_list)
{
	struct qnetd_client *client;
	struct qnetd_client *client_next;

	client = TAILQ_FIRST(clients_list);

	while (client != NULL) {
		client_next = TAILQ_NEXT(client, entries);

		free(client);

		client = client_next;
	}

	TAILQ_INIT(clients_list);
}

void
qnetd_clients_list_del(struct qnetd_clients_list *clients_list, struct qnetd_client *client)
{

	TAILQ_REMOVE(clients_list, client, entries);
	free(client);
}
