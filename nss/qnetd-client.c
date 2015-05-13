#include <sys/types.h>

#include <string.h>

#include "qnetd-client.h"

void
qnetd_client_init(struct qnetd_client *client, PRFileDesc *socket, PRNetAddr *addr,
    size_t max_receive_size, size_t max_send_size)
{

	memset(client, 0, sizeof(*client));
	client->socket = socket;
	client->conn_state = QNETD_CLIENT_CONN_STATE_RECEIVING_MSG_HEADER;
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

PRInt16
qnetd_client_conn_state_to_poll_event(enum qnetd_client_conn_state state)
{
	PRInt16 res;

	res = 0;

	switch (state) {
	case QNETD_CLIENT_CONN_STATE_SENDING_MSG:
		res = PR_POLL_WRITE;
		break;
	case QNETD_CLIENT_CONN_STATE_RECEIVING_MSG_HEADER:
	case QNETD_CLIENT_CONN_STATE_RECEIVING_MSG_VALUE:
	case QNETD_CLIENT_CONN_STATE_SKIPPING_MSG:
		res = PR_POLL_READ;
		break;
	case QNETD_CLIENT_CONN_STATE_SSL_HANDSHAKE:
		res = PR_POLL_WRITE | PR_POLL_READ;
		break;
	default:
		res = 0;
		break;
	}

	return (res);
}
