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
	TLV_OPT_TLS_CLIENT_CERT_REQUIRED = 3,
	TLV_OPT_SUPPORTED_MESSAGES = 4,
	TLV_OPT_SUPPORTED_OPTIONS = 5,
	TLV_OPT_REPLY_ERROR_CODE = 6,
};

enum tlv_tls_supported {
	TLV_TLS_UNSUPPORTED = 0,
	TLV_TLS_SUPPORTED = 1,
	TLV_TLS_REQUIRED = 2,
};

enum tlv_reply_error_code {
	TLV_REPLY_ERROR_CODE_NO_ERROR = 0,
	TLV_REPLY_ERROR_CODE_UNSUPPORTED_NEEDED_MESSAGE = 1,
	TLV_REPLY_ERROR_CODE_UNSUPPORTED_NEEDED_OPTION = 2,
	TLV_REPLY_ERROR_CODE_TLS_REQUIRED = 3,
	TLV_REPLY_ERROR_CODE_UNSUPPORTED_MESSAGE = 4,
	TLV_REPLY_ERROR_CODE_MESSAGE_TOO_LONG = 5,
	TLV_REPLY_ERROR_CODE_PREINIT_REQUIRED = 6,
	TLV_REPLY_ERROR_CODE_DOESNT_CONTAIN_REQUIRED_OPTION = 7,
	TLV_REPLY_ERROR_CODE_UNEXPECTED_MESSAGE = 8,
	TLV_REPLY_ERROR_CODE_ERROR_DECODING_MSG = 9,
};

struct tlv_iterator {
	const struct dynar *msg;
	size_t current_pos;
	size_t msg_header_len;
};

extern int			 tlv_add(struct dynar *msg, enum tlv_opt_type opt_type, uint16_t opt_len,
    const void *value);

extern int			 tlv_add_u32(struct dynar *msg, enum tlv_opt_type opt_type, uint32_t u32);

extern int			 tlv_add_u8(struct dynar *msg, enum tlv_opt_type opt_type, uint8_t u8);

extern int			 tlv_add_u16(struct dynar *msg, enum tlv_opt_type opt_type, uint16_t u16);

extern int			 tlv_add_string(struct dynar *msg, enum tlv_opt_type opt_type, const char *str);

extern int			 tlv_add_u16_array(struct dynar *msg, enum tlv_opt_type opt_type,
    const uint16_t *array, size_t array_size);

extern int			 tlv_add_supported_options(struct dynar *msg,
    const enum tlv_opt_type *supported_options, size_t no_supported_options);

extern int			 tlv_add_msg_seq_number(struct dynar *msg, uint32_t msg_seq_number);

extern int			 tlv_add_cluster_name(struct dynar *msg, const char *cluster_name);

extern int			 tlv_add_tls_supported(struct dynar *msg, enum tlv_tls_supported tls_supported);

extern int			 tlv_add_tls_client_cert_required(struct dynar *msg, int tls_client_cert_required);

extern int			 tlv_add_reply_error_code(struct dynar *msg, enum tlv_reply_error_code error_code);

extern void			 tlv_iter_init(const struct dynar *msg, size_t msg_header_len,
    struct tlv_iterator *tlv_iter);

extern enum tlv_opt_type	 tlv_iter_get_type(const struct tlv_iterator *tlv_iter);

extern uint16_t			 tlv_iter_get_len(const struct tlv_iterator *tlv_iter);

extern const char		*tlv_iter_get_data(const struct tlv_iterator *tlv_iter);

extern int			 tlv_iter_next(struct tlv_iterator *tlv_iter);

extern int			 tlv_iter_decode_u8(struct tlv_iterator *tlv_iter, uint8_t *res);

extern int			 tlv_iter_decode_u32(struct tlv_iterator *tlv_iter, uint32_t *res);

extern int			 tlv_iter_decode_str(struct tlv_iterator *tlv_iter, char **str, size_t *str_len);

extern void			 tlv_get_supported_options(enum tlv_opt_type **supported_options,
    size_t *no_supported_options);

#ifdef __cplusplus
}
#endif

#endif /* _TLV_H_ */
