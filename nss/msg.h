#ifndef _MSG_H_
#define _MSG_H_

#include <sys/types.h>
#include <inttypes.h>

#include "dynar.h"

#ifdef __cplusplus
extern "C" {
#endif

enum msg_type {
	MSG_TYPE_PREINIT = 0,
	MSG_TYPE_PREINIT_REPLY = 1,
};

extern size_t		msg_create_preinit(struct dynar *msg, const char *cluster_name,
    uint32_t msg_seq_number);

extern size_t		msg_get_header_length(void);

extern uint32_t		msg_get_len(struct dynar *msg);

extern enum msg_type	msg_get_type(struct dynar *msg);

extern int		msg_is_valid_msg_type(struct dynar *msg);

#ifdef __cplusplus
}
#endif

#endif /* _MSG_H_ */
