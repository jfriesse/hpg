#ifndef _QNETD_POLL_ARRAY_H_
#define _QNETD_POLL_ARRAY_H_

#include <sys/types.h>
#include <inttypes.h>

#include <nspr.h>

#include "qnetd-client.h"
#include "qnetd-clients-list.h"

#ifdef __cplusplus
extern "C" {
#endif

struct qnetd_poll_array {
	PRPollDesc *array;
	unsigned int allocated;
	unsigned int items;
};


extern void		 qnetd_poll_array_init(struct qnetd_poll_array *poll_array);

extern void		 qnetd_poll_array_destroy(struct qnetd_poll_array *poll_array);

extern void		 qnetd_poll_array_clean(struct qnetd_poll_array *poll_array);

extern unsigned int	 qnetd_poll_array_size(struct qnetd_poll_array *poll_array);

extern PRPollDesc	*qnetd_poll_array_add(struct qnetd_poll_array *poll_array);

extern PRPollDesc 	*qnetd_poll_array_get(const struct qnetd_poll_array *poll_array, unsigned int pos);

extern PRPollDesc	*qnetd_poll_array_create_from_clients_list(struct qnetd_poll_array *poll_array,
    const struct qnetd_clients_list *clients_list, PRFileDesc *extra_fd, PRInt16 extra_fd_in_flags);

#ifdef __cplusplus
}
#endif

#endif /* _QNETD_POLL_ARRAY_H_ */
