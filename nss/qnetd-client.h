#ifndef _QNETD_CLIENT_H_
#define _QNETD_CLIENT_H_

#include <sys/types.h>

#include <sys/queue.h>
#include <inttypes.h>

#include <nspr.h>
#include "dynar.h"

#ifdef __cplusplus
extern "C" {
#endif

enum qnetd_client_conn_state {
	QNETD_CLIENT_CONN_STATE_SENDING_MSG,
	QNETD_CLIENT_CONN_STATE_RECEIVING_MSG_HEADER,
	QNETD_CLIENT_CONN_STATE_RECEIVING_MSG_VALUE,
	QNETD_CLIENT_CONN_STATE_SKIPPING_MSG,
	QNETD_CLIENT_CONN_STATE_SSL_HANDSHAKE,
};

struct qnetd_client {
	PRFileDesc *socket;
	PRNetAddr addr;
	enum qnetd_client_conn_state conn_state;
	struct dynar receive_buffer;
	struct dynar send_buffer;
	size_t msg_already_received_bytes;
	size_t msg_already_sent_bytes;
	TAILQ_ENTRY(qnetd_client) entries;
};

extern void		qnetd_client_init(struct qnetd_client *client, PRFileDesc *socket, PRNetAddr *addr,
    size_t max_receive_size, size_t max_send_size);

extern void		qnetd_client_destroy(struct qnetd_client *client);

extern PRInt16		qnetd_client_conn_state_to_poll_event(enum qnetd_client_conn_state state);

#ifdef __cplusplus
}
#endif

#endif /* _QNETD_CLIENT_H_ */
