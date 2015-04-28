#ifndef _QNETD_CLIENTS_ARRAY_H_
#define _QNETD_CLIENTS_ARRAY_H_

#include <sys/types.h>
#include <inttypes.h>

#include "qnetd-client.h"

#ifdef __cplusplus
extern "C" {
#endif

struct qnetd_clients_array {
	struct qnetd_client *array;
	unsigned int items;
	unsigned int allocated;
};


extern void			 qnetd_clients_array_init(struct qnetd_clients_array *clients_array);
extern void			 qnetd_clients_array_destroy(struct qnetd_clients_array *clients_array);
extern struct qnetd_client	*qnetd_clients_array_add(struct qnetd_clients_array *clients_array);

extern struct qnetd_client	*qnetd_clients_array_get(const struct qnetd_clients_array *clients_array,
    unsigned int pos);

extern unsigned int		 qnetd_clients_array_size(const struct qnetd_clients_array *clients_array);

extern void			 qnetd_clients_array_del(struct qnetd_clients_array *clients_array,
    unsigned int pos);

#ifdef __cplusplus
}
#endif

#endif /* _QNETD_CLIENTS_ARRAY_H_ */
