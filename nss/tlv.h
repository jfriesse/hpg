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
	TLV_OPT_TLS_SUPPORTED = 2,
};

enum tlv_tls_supported {
	TLV_TLS_UNSUPPORTED = 0,
	TLV_TLS_SUPPORTED = 1,
	TLV_TLS_REQUIRED = 2,
};

struct tlv_iterator {
	const struct dynar *msg;
	size_t current_pos;
	size_t msg_header_len;
};

extern int			 tlv_add(struct dynar *msg, enum tlv_opt_type opt_type, uint16_t opt_len,
    const void *value);

extern int			 tlv_add_u32(struct dynar *msg, enum tlv_opt_type opt_type, uint32_t u32);

extern int			 tlv_add_string(struct dynar *msg, enum tlv_opt_type opt_type, const char *str);

extern int			 tlv_add_msg_seq_number(struct dynar *msg, uint32_t msg_seq_number);

extern int			 tlv_add_cluster_name(struct dynar *msg, const char *cluster_name);

extern int			 tlv_add_tls_supported(struct dynar *msg, enum tlv_tls_supported tls_supported);

extern void			 tlv_iter_init(const struct dynar *msg, size_t msg_header_len,
    struct tlv_iterator *tlv_iter);

extern enum tlv_opt_type	 tlv_iter_get_type(const struct tlv_iterator *tlv_iter);

extern uint16_t			 tlv_iter_get_len(const struct tlv_iterator *tlv_iter);

extern const char		*tlv_iter_get_data(const struct tlv_iterator *tlv_iter);

extern int			 tlv_iter_next(struct tlv_iterator *tlv_iter);

extern int			 tlv_iter_decode_u32(struct tlv_iterator *tlv_iter, uint32_t *res);

extern int			 tlv_iter_decode_str(struct tlv_iterator *tlv_iter, char **str, size_t *str_len);

#ifdef __cplusplus
}
#endif

#endif /* _TLV_H_ */
