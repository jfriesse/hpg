#include <sys/types.h>
#include <arpa/inet.h>

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "msg.h"

#define MSG_TYPE_LENGTH		2
#define MSG_LENGTH_LENGTH	4

#define MSG_STATIC_SUPPORTED_MESSAGES_SIZE	10

enum msg_type msg_static_supported_messages[MSG_STATIC_SUPPORTED_MESSAGES_SIZE] = {
    MSG_TYPE_PREINIT,
    MSG_TYPE_PREINIT_REPLY,
    MSG_TYPE_STARTTLS,
    MSG_TYPE_INIT,
    MSG_TYPE_INIT_REPLY,
    MSG_TYPE_SERVER_ERROR,
    MSG_TYPE_SET_OPTION,
    MSG_TYPE_SET_OPTION_REPLY,
    MSG_TYPE_ECHO_REQUEST,
    MSG_TYPE_ECHO_REPLY,
};

size_t
msg_get_header_length(void)
{
	return (MSG_TYPE_LENGTH + MSG_LENGTH_LENGTH);
}

static void
msg_add_type(struct dynar *msg, enum msg_type type)
{
	uint16_t ntype;

	ntype = htons((uint16_t)type);
	dynar_cat(msg, &ntype, sizeof(ntype));
}

enum msg_type
msg_get_type(const struct dynar *msg)
{
	uint16_t ntype;
	uint16_t type;

	memcpy(&ntype, dynar_data(msg), sizeof(ntype));
	type = ntohs(ntype);

	return (type);
}

/*
 * We don't know size of message before call of this function, so zero is
 * added. Real value is set afterwards by msg_set_len.
 */
static void
msg_add_len(struct dynar *msg)
{
	uint32_t len;

	len = 0;
	dynar_cat(msg, &len, sizeof(len));
}

static void
msg_set_len(struct dynar *msg, uint32_t len)
{
	uint32_t nlen;

	nlen = htonl(len);
	memcpy(dynar_data(msg) + MSG_TYPE_LENGTH, &nlen, sizeof(nlen));
}

/*
 * Used only for echo reply msg. All other messages should use msg_add_type.
 */
static void
msg_set_type(struct dynar *msg, enum msg_type type)
{
	uint16_t ntype;

	ntype = htons((uint16_t)type);
	memcpy(dynar_data(msg), &ntype, sizeof(ntype));
}

uint32_t
msg_get_len(const struct dynar *msg)
{
	uint32_t nlen;
	uint32_t len;

	memcpy(&nlen, dynar_data(msg) + MSG_TYPE_LENGTH, sizeof(nlen));
	len = ntohl(nlen);

	return (len);
}


size_t
msg_create_preinit(struct dynar *msg, const char *cluster_name, int add_msg_seq_number, uint32_t msg_seq_number)
{

	dynar_clean(msg);

	msg_add_type(msg, MSG_TYPE_PREINIT);
	msg_add_len(msg);

	if (add_msg_seq_number) {
		if (tlv_add_msg_seq_number(msg, msg_seq_number) == -1) {
			goto small_buf_err;
		}
	}

	if (tlv_add_cluster_name(msg, cluster_name) == -1) {
		goto small_buf_err;
	}

	msg_set_len(msg, dynar_size(msg) - (MSG_TYPE_LENGTH + MSG_LENGTH_LENGTH));

	return (dynar_size(msg));

small_buf_err:
	return (0);
}

size_t
msg_create_preinit_reply(struct dynar *msg, int add_msg_seq_number, uint32_t msg_seq_number,
    enum tlv_tls_supported tls_supported, int tls_client_cert_required)
{

	dynar_clean(msg);

	msg_add_type(msg, MSG_TYPE_PREINIT_REPLY);
	msg_add_len(msg);

	if (add_msg_seq_number) {
		if (tlv_add_msg_seq_number(msg, msg_seq_number) == -1) {
			goto small_buf_err;
		}
	}

	if (tlv_add_tls_supported(msg, tls_supported) == -1) {
		goto small_buf_err;
	}

	if (tlv_add_tls_client_cert_required(msg, tls_client_cert_required) == -1) {
		goto small_buf_err;
	}

	msg_set_len(msg, dynar_size(msg) - (MSG_TYPE_LENGTH + MSG_LENGTH_LENGTH));

	return (dynar_size(msg));

small_buf_err:
	return (0);
}

size_t
msg_create_starttls(struct dynar *msg, int add_msg_seq_number, uint32_t msg_seq_number)
{

	dynar_clean(msg);

	msg_add_type(msg, MSG_TYPE_STARTTLS);
	msg_add_len(msg);

	if (add_msg_seq_number) {
		if (tlv_add_msg_seq_number(msg, msg_seq_number) == -1) {
			goto small_buf_err;
		}
	}

	msg_set_len(msg, dynar_size(msg) - (MSG_TYPE_LENGTH + MSG_LENGTH_LENGTH));

	return (dynar_size(msg));

small_buf_err:
	return (0);
}

size_t
msg_create_server_error(struct dynar *msg, int add_msg_seq_number, uint32_t msg_seq_number,
    enum tlv_reply_error_code reply_error_code)
{

	dynar_clean(msg);

	msg_add_type(msg, MSG_TYPE_SERVER_ERROR);
	msg_add_len(msg);

	if (add_msg_seq_number) {
		if (tlv_add_msg_seq_number(msg, msg_seq_number) == -1) {
			goto small_buf_err;
		}
	}

	if (tlv_add_reply_error_code(msg, reply_error_code) == -1) {
		goto small_buf_err;
	}

	msg_set_len(msg, dynar_size(msg) - (MSG_TYPE_LENGTH + MSG_LENGTH_LENGTH));

	return (dynar_size(msg));

small_buf_err:
	return (0);
}

static uint16_t *
msg_convert_msg_type_array_to_u16_array(const enum msg_type *msg_type_array, size_t array_size)
{
	uint16_t *u16a;
	size_t i;

	u16a = malloc(sizeof(*u16a) * array_size);
	if (u16a == NULL) {
		return (NULL);
	}

	for (i = 0; i < array_size; i++) {
		u16a[i] = (uint16_t)msg_type_array[i];
	}

	return (u16a);
}

size_t
msg_create_init(struct dynar *msg, int add_msg_seq_number, uint32_t msg_seq_number,
    const enum msg_type *supported_msgs, size_t no_supported_msgs,
    const enum tlv_opt_type *supported_opts, size_t no_supported_opts, uint32_t node_id)
{
	uint16_t *u16a;
	int res;

	u16a = NULL;

	dynar_clean(msg);

	msg_add_type(msg, MSG_TYPE_INIT);
	msg_add_len(msg);

	if (add_msg_seq_number) {
		if (tlv_add_msg_seq_number(msg, msg_seq_number) == -1) {
			goto small_buf_err;
		}
	}

	if (supported_msgs != NULL && no_supported_msgs > 0) {
		u16a = msg_convert_msg_type_array_to_u16_array(supported_msgs, no_supported_msgs);

		if (u16a == NULL) {
			goto small_buf_err;
		}

		res = tlv_add_u16_array(msg, TLV_OPT_SUPPORTED_MESSAGES, u16a, no_supported_msgs);

		free(u16a);

		if (res == -1) {
			goto small_buf_err;
		}
	}

	if (supported_opts != NULL && no_supported_opts > 0) {
		if (tlv_add_supported_options(msg, supported_opts, no_supported_opts) == -1) {
			goto small_buf_err;
		}
	}

        if (tlv_add_node_id(msg, node_id) == -1) {
		goto small_buf_err;
        }

	msg_set_len(msg, dynar_size(msg) - (MSG_TYPE_LENGTH + MSG_LENGTH_LENGTH));

	return (dynar_size(msg));

small_buf_err:
	return (0);
}

size_t
msg_create_init_reply(struct dynar *msg, int add_msg_seq_number, uint32_t msg_seq_number,
    const enum msg_type *supported_msgs, size_t no_supported_msgs,
    const enum tlv_opt_type *supported_opts, size_t no_supported_opts,
    size_t server_maximum_request_size, size_t server_maximum_reply_size,
    const enum tlv_decision_algorithm_type *supported_decision_algorithms, size_t no_supported_decision_algorithms)
{
	uint16_t *u16a;
	int res;

	u16a = NULL;

	dynar_clean(msg);

	msg_add_type(msg, MSG_TYPE_INIT_REPLY);
	msg_add_len(msg);

	if (supported_msgs != NULL && no_supported_msgs > 0) {
		u16a = msg_convert_msg_type_array_to_u16_array(supported_msgs, no_supported_msgs);

		if (u16a == NULL) {
			goto small_buf_err;
		}

		res = tlv_add_u16_array(msg, TLV_OPT_SUPPORTED_MESSAGES, u16a, no_supported_msgs);

		free(u16a);

		if (res == -1) {
			goto small_buf_err;
		}
	}

	if (supported_opts != NULL && no_supported_opts > 0) {
		if (tlv_add_supported_options(msg, supported_opts, no_supported_opts) == -1) {
			goto small_buf_err;
		}
	}

	if (add_msg_seq_number) {
		if (tlv_add_msg_seq_number(msg, msg_seq_number) == -1) {
			goto small_buf_err;
		}
	}

	if (tlv_add_server_maximum_request_size(msg, server_maximum_request_size) == -1) {
		goto small_buf_err;
	}

	if (tlv_add_server_maximum_reply_size(msg, server_maximum_reply_size) == -1) {
		goto small_buf_err;
	}

	if (supported_decision_algorithms != NULL && no_supported_decision_algorithms > 0) {
		if (tlv_add_supported_decision_algorithms(msg, supported_decision_algorithms,
		    no_supported_decision_algorithms) == -1) {
			goto small_buf_err;
		}
	}

	msg_set_len(msg, dynar_size(msg) - (MSG_TYPE_LENGTH + MSG_LENGTH_LENGTH));

	return (dynar_size(msg));

small_buf_err:
	return (0);
}

size_t
msg_create_set_option(struct dynar *msg, int add_msg_seq_number, uint32_t msg_seq_number,
    int add_decision_algorithm, enum tlv_decision_algorithm_type decision_algorithm,
    int add_heartbeat_interval, uint32_t heartbeat_interval)
{

	dynar_clean(msg);

	msg_add_type(msg, MSG_TYPE_SET_OPTION);
	msg_add_len(msg);

	if (add_msg_seq_number) {
		if (tlv_add_msg_seq_number(msg, msg_seq_number) == -1) {
			goto small_buf_err;
		}
	}

	if (add_decision_algorithm) {
		if (tlv_add_decision_algorithm(msg, decision_algorithm) == -1) {
			goto small_buf_err;
		}
	}

	if (add_heartbeat_interval) {
		if (tlv_add_heartbeat_interval(msg, heartbeat_interval) == -1) {
			goto small_buf_err;
		}
	}

	msg_set_len(msg, dynar_size(msg) - (MSG_TYPE_LENGTH + MSG_LENGTH_LENGTH));

	return (dynar_size(msg));

small_buf_err:
	return (0);
}

size_t
msg_create_set_option_reply(struct dynar *msg, int add_msg_seq_number, uint32_t msg_seq_number,
    enum tlv_decision_algorithm_type decision_algorithm, uint32_t heartbeat_interval)
{

	dynar_clean(msg);

	msg_add_type(msg, MSG_TYPE_SET_OPTION_REPLY);
	msg_add_len(msg);

	if (add_msg_seq_number) {
		if (tlv_add_msg_seq_number(msg, msg_seq_number) == -1) {
			goto small_buf_err;
		}
	}

	if (tlv_add_decision_algorithm(msg, decision_algorithm) == -1) {
		goto small_buf_err;
	}

	if (tlv_add_heartbeat_interval(msg, heartbeat_interval) == -1) {
		goto small_buf_err;
	}

	msg_set_len(msg, dynar_size(msg) - (MSG_TYPE_LENGTH + MSG_LENGTH_LENGTH));

	return (dynar_size(msg));

small_buf_err:
	return (0);
}

size_t
msg_create_echo_request(struct dynar *msg, int add_msg_seq_number, uint32_t msg_seq_number)
{

	dynar_clean(msg);

	msg_add_type(msg, MSG_TYPE_ECHO_REQUEST);
	msg_add_len(msg);

	if (add_msg_seq_number) {
		if (tlv_add_msg_seq_number(msg, msg_seq_number) == -1) {
			goto small_buf_err;
		}
	}

	msg_set_len(msg, dynar_size(msg) - (MSG_TYPE_LENGTH + MSG_LENGTH_LENGTH));

	return (dynar_size(msg));

small_buf_err:
	return (0);
}

size_t
msg_create_echo_reply(struct dynar *msg, const struct dynar *echo_request_msg)
{

	dynar_clean(msg);

	if (dynar_cat(msg, dynar_data(echo_request_msg), dynar_size(echo_request_msg)) == -1) {
		goto small_buf_err;
	}

	msg_set_type(msg, MSG_TYPE_ECHO_REPLY);

	return (dynar_size(msg));

small_buf_err:
	return (0);
}

int
msg_is_valid_msg_type(const struct dynar *msg)
{
	enum msg_type type;
	size_t i;

	type = msg_get_type(msg);

	for (i = 0; i < MSG_STATIC_SUPPORTED_MESSAGES_SIZE; i++) {
		if (msg_static_supported_messages[i] == type) {
			return (1);
		}
	}

	return (0);
}

void
msg_decoded_init(struct msg_decoded *decoded_msg)
{

	memset(decoded_msg, 0, sizeof(*decoded_msg));
}

void
msg_decoded_destroy(struct msg_decoded *decoded_msg)
{

	free(decoded_msg->cluster_name);
	free(decoded_msg->supported_messages);
	free(decoded_msg->supported_options);

	msg_decoded_init(decoded_msg);
}

/*
 *  0 - No error
 * -1 - option with invalid length
 * -2 - Unable to allocate memory
 * -3 - Inconsistent msg (tlv len > msg size)
 * -4 - invalid option content
 */
int
msg_decode(const struct dynar *msg, struct msg_decoded *decoded_msg)
{
	struct tlv_iterator tlv_iter;
	uint16_t *u16a;
	uint32_t u32;
	size_t zi;
	enum tlv_opt_type opt_type;
	int iter_res;
	int res;

	msg_decoded_destroy(decoded_msg);

	decoded_msg->type = msg_get_type(msg);

	tlv_iter_init(msg, msg_get_header_length(), &tlv_iter);

	while ((iter_res = tlv_iter_next(&tlv_iter)) > 0) {
		opt_type = tlv_iter_get_type(&tlv_iter);

		switch (opt_type) {
		case TLV_OPT_MSG_SEQ_NUMBER:
			if (tlv_iter_decode_u32(&tlv_iter, &decoded_msg->seq_number) != 0) {
				return (-1);
			}

			decoded_msg->seq_number_set = 1;
			break;
		case TLV_OPT_CLUSTER_NAME:
			if (tlv_iter_decode_str(&tlv_iter, &decoded_msg->cluster_name,
			    &decoded_msg->cluster_name_len) != 0) {
				return (-2);
			}
			break;
		case TLV_OPT_TLS_SUPPORTED:
			if ((res = tlv_iter_decode_tls_supported(&tlv_iter, &decoded_msg->tls_supported)) != 0) {
				return (res);
			}

			decoded_msg->tls_supported_set = 1;
			break;
		case TLV_OPT_TLS_CLIENT_CERT_REQUIRED:
			if (tlv_iter_decode_client_cert_required(&tlv_iter, &decoded_msg->tls_client_cert_required) != 0) {
				return (-1);
			}

			decoded_msg->tls_client_cert_required_set = 1;
			break;
		case TLV_OPT_SUPPORTED_MESSAGES:
			free(decoded_msg->supported_messages);

			if ((res = tlv_iter_decode_u16_array(&tlv_iter, &u16a,
			    &decoded_msg->no_supported_messages)) != 0) {
				return (res);
			}

			decoded_msg->supported_messages = malloc(sizeof(enum msg_type) * decoded_msg->no_supported_messages);
			if (decoded_msg->supported_messages == NULL) {
				free(u16a);
				return (-2);
			}

			for (zi = 0; zi < decoded_msg->no_supported_messages; zi++) {
				decoded_msg->supported_messages[zi] = (enum msg_type)u16a[zi];
			}

			free(u16a);
			break;
		case TLV_OPT_SUPPORTED_OPTIONS:
			free(decoded_msg->supported_options);

			if ((res = tlv_iter_decode_supported_options(&tlv_iter, &decoded_msg->supported_options,
			    &decoded_msg->no_supported_options)) != 0) {
				return (res);
			}
			break;
		case TLV_OPT_REPLY_ERROR_CODE:
			if (tlv_iter_decode_reply_error_code(&tlv_iter, &decoded_msg->reply_error_code) != 0) {
				return (-1);
			}

			decoded_msg->reply_error_code_set = 1;
			break;
		case TLV_OPT_SERVER_MAXIMUM_REQUEST_SIZE:
			if (tlv_iter_decode_u32(&tlv_iter, &u32) != 0) {
				return (-1);
			}

			decoded_msg->server_maximum_request_size_set = 1;
			decoded_msg->server_maximum_request_size = u32;
			break;
		case TLV_OPT_SERVER_MAXIMUM_REPLY_SIZE:
			if (tlv_iter_decode_u32(&tlv_iter, &u32) != 0) {
				return (-1);
			}

			decoded_msg->server_maximum_reply_size_set = 1;
			decoded_msg->server_maximum_reply_size = u32;
			break;
		case TLV_OPT_NODE_ID:
			if (tlv_iter_decode_u32(&tlv_iter, &u32) != 0) {
				return (-1);
			}

			decoded_msg->node_id_set = 1;
			decoded_msg->node_id = u32;
			break;
		case TLV_OPT_SUPPORTED_DECISION_ALGORITHMS:
			free(decoded_msg->supported_decision_algorithms);

			if ((res = tlv_iter_decode_supported_decision_algorithms(&tlv_iter,
			    &decoded_msg->supported_decision_algorithms,
			    &decoded_msg->no_supported_decision_algorithms)) != 0) {
				return (res);
			}
			break;
		case TLV_OPT_DECISION_ALGORITHM:
			if (tlv_iter_decode_decision_algorithm(&tlv_iter, &decoded_msg->decision_algorithm) != 0) {
				return (-1);
			}

			decoded_msg->decision_algorithm_set = 1;
			break;
		case TLV_OPT_HEARTBEAT_INTERVAL:
			if (tlv_iter_decode_u32(&tlv_iter, &u32) != 0) {
				return (-1);
			}

			decoded_msg->heartbeat_interval_set = 1;
			decoded_msg->heartbeat_interval = u32;
			break;
		default:
			/*
			 * Unknown option
			 */
			break;
		}
	}

	if (iter_res != 0) {
		return (-3);
	}

	return (0);
}

void
msg_get_supported_messages(enum msg_type **supported_messages, size_t *no_supported_messages)
{

	*supported_messages = msg_static_supported_messages;
	*no_supported_messages = MSG_STATIC_SUPPORTED_MESSAGES_SIZE;
}
