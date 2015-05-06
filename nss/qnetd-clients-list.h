#ifndef _QNETD_CLIENTS_LIST_H_
#define _QNETD_CLIENTS_LIST_H_

#include <sys/types.h>
#include <inttypes.h>

#include "qnetd-client.h"

#ifdef __cplusplus
extern "C" {
#endif

TAILQ_HEAD(qnetd_clients_list, qnetd_client);

extern void			 qnetd_clients_list_init(struct qnetd_clients_list *clients_list);

extern struct qnetd_client	*qnetd_clients_list_add(struct qnetd_clients_list *clients_list,
    PRFileDesc *socket, PRNetAddr *addr);

extern void			 qnetd_clients_list_free(struct qnetd_clients_list *clients_list);

extern void			 qnetd_clients_list_del(struct qnetd_clients_list *clients_list,
    struct qnetd_client *client);

#ifdef __cplusplus
}
#endif

#endif /* _QNETD_CLIENTS_LIST_H_ */
