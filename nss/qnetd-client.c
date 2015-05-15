#include <sys/types.h>

#include <string.h>

#include "qnetd-client.h"

void
qnetd_client_init(struct qnetd_client *client, PRFileDesc *socket, PRNetAddr *addr,
    size_t max_receive_size, size_t max_send_size)
{

	memset(client, 0, sizeof(*client));
	client->socket = socket;
	memcpy(&client->addr, addr, sizeof(*addr));
	dynar_init(&client->receive_buffer, max_receive_size);
	dynar_init(&client->send_buffer, max_send_size);
}

void
qnetd_client_destroy(struct qnetd_client *client)
{

	dynar_destroy(&client->receive_buffer);
	dynar_destroy(&client->send_buffer);
}
