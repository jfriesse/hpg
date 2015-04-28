#include <sys/types.h>
#include <arpa/inet.h>

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "qnetd-clients-array.h"

void
qnetd_clients_array_init(struct qnetd_clients_array *clients_array)
{

	memset(clients_array, 0, sizeof(*clients_array));
}

void
qnetd_clients_array_destroy(struct qnetd_clients_array *clients_array)
{

	free(clients_array->array);
	qnetd_clients_array_init(clients_array);
}

static int
qnetd_clients_array_realloc(struct qnetd_clients_array *clients_array,
    unsigned int new_clients_array_size)
{
	struct qnetd_client *new_clients_array;

	new_clients_array = realloc(clients_array->array,
	     sizeof(struct qnetd_client) * new_clients_array_size);

	if (new_clients_array == NULL) {
		return (-1);
	}

	clients_array->allocated = new_clients_array_size;
	clients_array->array = new_clients_array;

	return (0);
}

struct qnetd_client *
qnetd_clients_array_add(struct qnetd_clients_array *clients_array)
{

	if (clients_array->items >= clients_array->allocated) {
		if (qnetd_clients_array_realloc(clients_array, (clients_array->allocated * 2) + 1)) {
			return (NULL);
		}
	}

	clients_array->items++;

	return (&clients_array->array[clients_array->items - 1]);
}

static void
qnetd_clients_array_gc(struct qnetd_clients_array *clients_array)
{

	/*
	 * Realloc is expensive operation so make sure number of items is really smaller
	 * then allocated items
	 */
	if (clients_array->allocated > (clients_array->items * 3) + 1) {
		qnetd_clients_array_realloc(clients_array, (clients_array->items * 2) + 1);
	}
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
qnetd_clients_array_del(struct qnetd_clients_array *clients_array, unsigned int pos)
{
	struct qnetd_client *item;
	void *dst_ptr;
	void *last_item_end_ptr;
	void *src_ptr;

	item = qnetd_clients_array_get(clients_array, pos);
	if (item == NULL) {
		return ;
	}

	last_item_end_ptr = (void *)clients_array->array + (sizeof(*item) * clients_array->items);
	src_ptr = (void *)item + sizeof(*item);
	dst_ptr = (void *)item;

	memmove(dst_ptr, src_ptr, last_item_end_ptr - src_ptr);

	clients_array->items--;

	qnetd_clients_array_gc(clients_array);
}
