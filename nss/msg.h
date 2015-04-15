#ifndef _MSG_H_
#define _MSG_H_

#include <sys/types.h>
#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

enum msg_type {
	MSG_TYPE_PREINIT = 0,
	MSG_TYPE_PREINIT_REPLY = 1,
};

extern size_t	msg_create_preinit(char *msg, size_t msg_len, const char *cluster_name,
    uint32_t msg_seq_number);

#ifdef __cplusplus
}
#endif

#endif /* _MSG_H_ */
