#ifndef _MSG_H_
#define _MSG_H_

#include <sys/types.h>
#include <inttypes.h>

#include "dynar.h"
#include "tlv.h"

#ifdef __cplusplus
extern "C" {
#endif

enum msg_type {
	MSG_TYPE_PREINIT = 0,
	MSG_TYPE_PREINIT_REPLY = 1,
	MSG_TYPE_STARTTLS = 2,
	MSG_TYPE_INIT = 3,
	MSG_TYPE_INIT_REPLY = 4,
	MSG_TYPE_SERVER_ERROR = 5,
	MSG_TYPE_SET_OPTION = 6,
	MSG_TYPE_SET_OPTION_REPLY = 7,
};

struct msg_decoded {
	enum msg_type type;
	uint8_t seq_number_set;
	uint32_t seq_number;		// Only valid if seq_number_set != 0
	size_t cluster_name_len;
	char *cluster_name;		// Valid only if != NULL. Trailing \0 is added but not counted in cluster_name_len
	uint8_t tls_supported_set;
	enum tlv_tls_supported tls_supported;	// Valid only if tls_supported_set != 0.
	uint8_t tls_client_cert_required_set;
	uint8_t tls_client_cert_required;		// Valid only if tls_client_cert_required_set != 0
	size_t no_supported_messages;
	enum msg_type *supported_messages;	// Valid only if != NULL
	size_t no_supported_options;
	enum tlv_opt_type *supported_options;	// Valid only if != NULL
	uint8_t reply_error_code_set;
	enum tlv_reply_error_code reply_error_code;	// Valid only if reply_error_code_set != 0
	uint8_t server_maximum_request_size_set;
	size_t server_maximum_request_size;		// Valid only if server_maximum_request_size_set != 0
	uint8_t server_maximum_reply_size_set;
	size_t server_maximum_reply_size;		// Valid only if server_maximum_reply_size_set != 0
	uint8_t node_id_set;
	uint32_t node_id;
	size_t no_supported_decision_algorithms;
	enum tlv_decision_algorithm_type *supported_decision_algorithms;	// Valid only if != NULL
	uint8_t decision_algorithm_set;
	enum tlv_decision_algorithm_type decision_algorithm;		// Valid only if decision_algorithm_set != 0
	uint8_t heartbeat_interval_set;
	uint32_t heartbeat_interval;					// Valid only if heartbeat_interval_set != 0
};

extern size_t		msg_create_preinit(struct dynar *msg, const char *cluster_name,
    int add_msg_seq_number, uint32_t msg_seq_number);

extern size_t		msg_create_preinit_reply(struct dynar *msg, int add_msg_seq_number,
    uint32_t msg_seq_number, enum tlv_tls_supported tls_supported, int tls_client_cert_required);

extern size_t		msg_create_starttls(struct dynar *msg, int add_msg_seq_number,
    uint32_t msg_seq_number);

extern size_t		msg_create_init(struct dynar *msg, int add_msg_seq_number, uint32_t msg_seq_number,
    const enum msg_type *supported_msgs, size_t no_supported_msgs,
    const enum tlv_opt_type *supported_opts, size_t no_supported_opts, uint32_t node_id);

extern size_t		msg_create_server_error(struct dynar *msg, int add_msg_seq_number, uint32_t msg_seq_number,
    enum tlv_reply_error_code reply_error_code);

extern size_t		msg_create_init_reply(struct dynar *msg, int add_msg_seq_number, uint32_t msg_seq_number,
    const enum msg_type *supported_msgs, size_t no_supported_msgs,
    const enum tlv_opt_type *supported_opts, size_t no_supported_opts,
    size_t server_maximum_request_size, size_t server_maximum_reply_size,
    const enum tlv_decision_algorithm_type *supported_decision_algorithms, size_t no_supported_decision_algorithms);

extern size_t		msg_create_set_option(struct dynar *msg,
    int add_msg_seq_number, uint32_t msg_seq_number,
    int add_decision_algorithm, enum tlv_decision_algorithm_type decision_algorithm,
    int add_heartbeat_interval, uint32_t heartbeat_interval);

extern size_t		msg_create_set_option_reply(struct dynar *msg,
    int add_msg_seq_number, uint32_t msg_seq_number,
    enum tlv_decision_algorithm_type decision_algorithm, uint32_t heartbeat_interval);

extern size_t		msg_get_header_length(void);

extern uint32_t		msg_get_len(const struct dynar *msg);

extern enum msg_type	msg_get_type(const struct dynar *msg);

extern int		msg_is_valid_msg_type(const struct dynar *msg);

extern void		msg_decoded_init(struct msg_decoded *decoded_msg);

extern void		msg_decoded_destroy(struct msg_decoded *decoded_msg);

extern int		msg_decode(const struct dynar *msg, struct msg_decoded *decoded_msg);

extern void		msg_get_supported_messages(enum msg_type **supported_messages,
    size_t *no_supported_messages);

#ifdef __cplusplus
}
#endif

#endif /* _MSG_H_ */
