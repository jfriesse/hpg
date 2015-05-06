#ifndef _QNETD_CLIENT_H_
#define _QNETD_CLIENT_H_

#include <sys/types.h>

#include <sys/queue.h>
#include <inttypes.h>

#include <nspr.h>

#ifdef __cplusplus
extern "C" {
#endif

enum qnetd_client_state {
	QNETD_CLIENT_STATE_SENDING_MSG,
	QNETD_CLIENT_STATE_RECEIVING_MSG,
	QNETD_CLIENT_STATE_SSL_HANDSHAKE,
};

struct qnetd_client {
	PRFileDesc *socket;
	PRNetAddr addr;
	enum qnetd_client_state state;
	TAILQ_ENTRY(qnetd_client) entries;
};

extern void		qnetd_client_init(struct qnetd_client *client, PRFileDesc *socket, PRNetAddr *addr);

extern PRInt16		qnetd_client_state_to_poll_event(enum qnetd_client_state state);

#ifdef __cplusplus
}
#endif

#endif /* _QNETD_CLIENT_H_ */
