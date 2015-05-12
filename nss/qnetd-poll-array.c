#include <sys/types.h>
#include <arpa/inet.h>

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "qnetd-poll-array.h"

void
qnetd_poll_array_init(struct qnetd_poll_array *poll_array)
{

	memset(poll_array, 0, sizeof(*poll_array));
}

void
qnetd_poll_array_destroy(struct qnetd_poll_array *poll_array)
{

	free(poll_array->array);
	qnetd_poll_array_init(poll_array);
}

void
qnetd_poll_array_clean(struct qnetd_poll_array *poll_array)
{

	poll_array->items = 0;
}

static int
qnetd_poll_array_realloc(struct qnetd_poll_array *poll_array,
    unsigned int new_array_size)
{
	PRPollDesc *new_array;

	new_array = realloc(poll_array->array,
	    sizeof(PRPollDesc) * new_array_size);

	if (new_array == NULL) {
		return (-1);
	}

	poll_array->allocated = new_array_size;
	poll_array->array = new_array;

	return (0);
}

unsigned int
qnetd_poll_array_size(struct qnetd_poll_array *poll_array)
{

	return (poll_array->items);
}

PRPollDesc *
qnetd_poll_array_add(struct qnetd_poll_array *poll_array)
{

	if (qnetd_poll_array_size(poll_array) >= poll_array->allocated) {
		if (qnetd_poll_array_realloc(poll_array, (poll_array->allocated * 2) + 1)) {
			return (NULL);
		}
	}

	poll_array->items++;

	return (&poll_array->array[qnetd_poll_array_size(poll_array) - 1]);
}

static void
qnetd_poll_array_gc(struct qnetd_poll_array *poll_array)
{

	if (poll_array->allocated > (qnetd_poll_array_size(poll_array) * 3) + 1) {
		qnetd_poll_array_realloc(poll_array, (qnetd_poll_array_size(poll_array) * 2) + 1);
	}
}

PRPollDesc *
qnetd_poll_array_get(const struct qnetd_poll_array *poll_array, unsigned int pos)
{

	if (pos >= poll_array->items) {
		return (NULL);
	}

	return (&poll_array->array[pos]);
}

PRPollDesc *
qnetd_poll_array_create_from_clients_list(struct qnetd_poll_array *poll_array,
    const struct qnetd_clients_list *clients_list,
    PRFileDesc *extra_fd, PRInt16 extra_fd_in_flags)
{
	struct qnetd_client *client;
	PRPollDesc *poll_desc;

	qnetd_poll_array_clean(poll_array);

	if (extra_fd != NULL) {
		poll_desc = qnetd_poll_array_add(poll_array);
		if (poll_desc == NULL) {
			return (NULL);
		}

		poll_desc->fd = extra_fd;
		poll_desc->in_flags = extra_fd_in_flags;
		poll_desc->out_flags = 0;
	}

	TAILQ_FOREACH(client, clients_list, entries) {
		poll_desc = qnetd_poll_array_add(poll_array);
		if (poll_desc == NULL) {
			return (NULL);
		}
		poll_desc->fd = client->socket;
		poll_desc->in_flags = qnetd_client_conn_state_to_poll_event(client->conn_state);
		poll_desc->out_flags = 0;
	}

	qnetd_poll_array_gc(poll_array);

	return (poll_array->array);
}
