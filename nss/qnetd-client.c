#include <sys/types.h>

#include <string.h>

#include "qnetd-client.h"

void
qnetd_client_init(struct qnetd_client *client, PRFileDesc *socket, PRNetAddr *addr)
{

	memset(client, 0, sizeof(*client));
	client->socket = socket;
	client->state = QNETD_CLIENT_STATE_RECEIVING_MSG;
	memcpy(&client->addr, addr, sizeof(*addr));
}

PRInt16
qnetd_client_state_to_poll_event(enum qnetd_client_state state)
{
	PRInt16 res;

	res = 0;

	switch (state) {
	case QNETD_CLIENT_STATE_SENDING_MSG:
		res = PR_POLL_WRITE;
		break;
	case QNETD_CLIENT_STATE_RECEIVING_MSG:
		res = PR_POLL_READ;
		break;
	case QNETD_CLIENT_STATE_SSL_HANDSHAKE:
		res = PR_POLL_WRITE | PR_POLL_READ;
		break;
	default:
		res = 0;
		break;
	}

	return (res);
}
