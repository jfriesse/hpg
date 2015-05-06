#ifndef _TLV_H_
#define _TLV_H_

#include <sys/types.h>
#include <inttypes.h>

#include "dynar.h"

#ifdef __cplusplus
extern "C" {
#endif

enum tlv_opt_type {
	TLV_OPT_MSG_SEQ_NUMBER = 0,
	TLV_OPT_CLUSTER_NAME = 1,
};

extern int	tlv_add(struct dynar *msg, enum tlv_opt_type opt_type, uint16_t opt_len, const void *value);

extern int	tlv_add_u32(struct dynar *msg, enum tlv_opt_type opt_type, uint32_t u32);

extern int	tlv_add_string(struct dynar *msg, enum tlv_opt_type opt_type, const char *str);

extern int	tlv_add_msg_seq_number(struct dynar *msg, uint32_t msg_seq_number);

extern int	tlv_add_cluster_name(struct dynar *msg, const char *cluster_name);

#ifdef __cplusplus
}
#endif

#endif /* _TLV_H_ */
