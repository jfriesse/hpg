#ifndef _QNETD_CLIENT_H_
#define _QNETD_CLIENT_H_

#include <sys/types.h>

#include <sys/queue.h>
#include <inttypes.h>

#include <nspr.h>
#include "dynar.h"
#include "tlv.h"

#ifdef __cplusplus
extern "C" {
#endif

struct qnetd_client {
	PRFileDesc *socket;
	PRNetAddr addr;
	struct dynar receive_buffer;
	struct dynar send_buffer;
	size_t msg_already_received_bytes;
	size_t msg_already_sent_bytes;
	int sending_msg;	// Have message to sent
	int skipping_msg;	// When incorrect message was received skip it
	int tls_started;	// Set after TLS started
	int tls_peer_certificate_verified;	// Certificate is verified only once
	int preinit_received;
	int init_received;
	char *cluster_name;
	size_t cluster_name_len;
	enum tlv_reply_error_code skipping_msg_reason;
	TAILQ_ENTRY(qnetd_client) entries;
};

extern void		qnetd_client_init(struct qnetd_client *client, PRFileDesc *socket, PRNetAddr *addr,
    size_t max_receive_size, size_t max_send_size);

extern void		qnetd_client_destroy(struct qnetd_client *client);

#ifdef __cplusplus
}
#endif

#endif /* _QNETD_CLIENT_H_ */
