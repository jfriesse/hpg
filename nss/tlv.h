#ifndef _TLV_H_
#define _TLV_H_

#include <sys/types.h>
#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

enum tlv_opt_type {
	TLV_OPT_MSG_SEQ_NUMBER = 0,
	TLV_OPT_CLUSTER_NAME = 1,
};

extern int	tlv_add(char *msg, size_t msg_len, size_t *pos, enum tlv_opt_type opt_type,
    uint16_t opt_len, const void *value);

extern int	tlv_add_u32(char *msg, size_t msg_len, size_t *pos, enum tlv_opt_type opt_type,
    uint32_t u32);

extern int	tlv_add_string(char *msg, size_t msg_len, size_t *pos, enum tlv_opt_type opt_type,
    const char *str);

extern int	tlv_add_msg_seq_number(char *msg, size_t msg_len, size_t *pos, uint32_t msg_seq_number);

extern int	tlv_add_cluster_name(char *msg, size_t msg_len, size_t *pos, const char *cluster_name);

#ifdef __cplusplus
}
#endif

#endif /* _TLV_H_ */
