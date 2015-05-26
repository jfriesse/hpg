#include <sys/types.h>
#include <arpa/inet.h>

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "msg.h"

#define MSG_TYPE_LENGTH		2
#define MSG_LENGTH_LENGTH	4

#define MSG_STATIC_SUPPORTED_MESSAGES_SIZE	4

enum msg_type msg_static_supported_messages[MSG_STATIC_SUPPORTED_MESSAGES_SIZE] = {
    MSG_TYPE_PREINIT,
    MSG_TYPE_PREINIT_REPLY,
    MSG_TYPE_STARTTLS,
    MSG_TYPE_INIT,
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

size_t
msg_create_init(struct dynar *msg, int add_msg_seq_number, uint32_t msg_seq_number,
    const enum msg_type *supported_msgs, size_t no_supported_msgs,
    const enum tlv_opt_type *supported_opts, size_t no_supported_opts)
{

	dynar_clean(msg);

	msg_add_type(msg, MSG_TYPE_INIT);
	msg_add_len(msg);

	if (add_msg_seq_number) {
		if (tlv_add_msg_seq_number(msg, msg_seq_number) == -1) {
			goto small_buf_err;
		}
	}

	if (supported_msgs != NULL && no_supported_msgs > 0) {
		if (tlv_add_u16_array(msg, TLV_OPT_SUPPORTED_MESSAGES,
		    (uint16_t *)supported_msgs, no_supported_msgs) == -1) {
			goto small_buf_err;
		}
	}

	if (supported_opts != NULL && no_supported_opts > 0) {
		if (tlv_add_supported_options(msg, supported_opts, no_supported_opts) == -1) {
			goto small_buf_err;
		}
	}

	msg_set_len(msg, dynar_size(msg) - (MSG_TYPE_LENGTH + MSG_LENGTH_LENGTH));

	return (dynar_size(msg));

small_buf_err:
	return (0);
}

int
msg_is_valid_msg_type(const struct dynar *msg)
{
	enum msg_type type;
	int res;

	res = 0;
	type = msg_get_type(msg);

	switch (type) {
	case MSG_TYPE_PREINIT:
	case MSG_TYPE_PREINIT_REPLY:
	case MSG_TYPE_STARTTLS:
		res = 1;
		break;
	default:
		res = 0;
		break;
	}

	return (res);
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
	enum tlv_opt_type opt_type;
	int iter_res;

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
			if (tlv_iter_decode_u8(&tlv_iter, (uint8_t *)&decoded_msg->tls_supported) != 0) {
				return (-1);
			}

			if (decoded_msg->tls_supported != TLV_TLS_UNSUPPORTED &&
			    decoded_msg->tls_supported != TLV_TLS_SUPPORTED &&
			    decoded_msg->tls_supported != TLV_TLS_REQUIRED) {
				return (-4);
			}

			decoded_msg->tls_supported_set = 1;
			break;
		case TLV_OPT_TLS_CLIENT_CERT_REQUIRED:
			if (tlv_iter_decode_u8(&tlv_iter, (uint8_t *)&decoded_msg->tls_client_cert_required) != 0) {
				return (-1);
			}

			decoded_msg->tls_client_cert_required_set = 1;
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
