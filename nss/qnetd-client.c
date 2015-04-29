#include <sys/types.h>

#include <string.h>

#include "qnetd-client.h"

void
qnetd_client_init(struct qnetd_client *client, PRFileDesc *socket)
{

	memset(client, 0, sizeof(*client));
	client->socket = socket;
	client->state = QNETD_CLIENT_STATE_RECEIVING_MSG;
}
