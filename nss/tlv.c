#include <sys/types.h>
#include <arpa/inet.h>

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "tlv.h"

#define TLV_TYPE_LENGTH		2
#define TLV_LENGTH_LENGTH	2

int
tlv_add(struct dynar *msg, enum tlv_opt_type opt_type, uint16_t opt_len, const void *value)
{
	uint16_t nlen;
	uint16_t nopt_type;

	if (dynar_size(msg) + sizeof(nopt_type) + sizeof(nlen) + opt_len > dynar_max_size(msg)) {
		return (-1);
	}

	nopt_type = htons((uint16_t)opt_type);
	nlen = htons(opt_len);

	dynar_cat(msg, &nopt_type, sizeof(nopt_type));
	dynar_cat(msg, &nlen, sizeof(nlen));
	dynar_cat(msg, value, opt_len);

	return (0);
}

int
tlv_add_u32(struct dynar *msg, enum tlv_opt_type opt_type, uint32_t u32)
{
	uint32_t nu32;

	nu32 = htonl(u32);

	return (tlv_add(msg, opt_type, sizeof(nu32), &nu32));
}

int
tlv_add_u8(struct dynar *msg, enum tlv_opt_type opt_type, uint8_t u8)
{

	return (tlv_add(msg, opt_type, sizeof(u8), &u8));
}

int
tlv_add_string(struct dynar *msg, enum tlv_opt_type opt_type, const char *str)
{

	return (tlv_add(msg, opt_type, strlen(str), str));
}

int
tlv_add_msg_seq_number(struct dynar *msg, uint32_t msg_seq_number)
{

	return (tlv_add_u32(msg, TLV_OPT_MSG_SEQ_NUMBER, msg_seq_number));
}

int
tlv_add_cluster_name(struct dynar *msg, const char *cluster_name)
{

	return (tlv_add_string(msg, TLV_OPT_CLUSTER_NAME, cluster_name));
}

int
tlv_add_tls_supported(struct dynar *msg, enum tlv_tls_supported tls_supported)
{

	return (tlv_add_u8(msg, TLV_OPT_TLS_SUPPORTED, tls_supported));
}

int
tlv_add_tls_client_cert_required(struct dynar *msg, int tls_client_cert_required)
{

	return (tlv_add_u8(msg, TLV_OPT_TLS_CLIENT_CERT_REQUIRED, tls_client_cert_required));
}

void
tlv_iter_init(const struct dynar *msg, size_t msg_header_len, struct tlv_iterator *tlv_iter)
{

	tlv_iter->msg = msg;
	tlv_iter->current_pos = 0;
	tlv_iter->msg_header_len = msg_header_len;
}

enum tlv_opt_type
tlv_iter_get_type(const struct tlv_iterator *tlv_iter)
{
	uint16_t ntype;
	uint16_t type;

	memcpy(&ntype, dynar_data(tlv_iter->msg) + tlv_iter->current_pos, sizeof(ntype));
	type = ntohs(ntype);

	return (type);
}

uint16_t
tlv_iter_get_len(const struct tlv_iterator *tlv_iter)
{
	uint16_t nlen;
	uint16_t len;

	memcpy(&nlen, dynar_data(tlv_iter->msg) + tlv_iter->current_pos + TLV_TYPE_LENGTH, sizeof(nlen));
	len = ntohs(nlen);

	return (len);
}

const char *
tlv_iter_get_data(const struct tlv_iterator *tlv_iter)
{

	return (dynar_data(tlv_iter->msg) + tlv_iter->current_pos + TLV_TYPE_LENGTH + TLV_LENGTH_LENGTH);
}

int
tlv_iter_next(struct tlv_iterator *tlv_iter)
{
	uint16_t len;

	if (tlv_iter->current_pos == 0) {
		tlv_iter->current_pos = tlv_iter->msg_header_len;

		goto check_tlv_validity;
	}

	len = tlv_iter_get_len(tlv_iter);

	if (tlv_iter->current_pos + TLV_TYPE_LENGTH + TLV_LENGTH_LENGTH + len >= dynar_size(tlv_iter->msg)) {
		return (0);
	}

	tlv_iter->current_pos += TLV_TYPE_LENGTH + TLV_LENGTH_LENGTH + len;

check_tlv_validity:
	/*
	 * Check if tlv is valid = is not larger than whole message
	 */
	len = tlv_iter_get_len(tlv_iter);

	if (tlv_iter->current_pos + TLV_TYPE_LENGTH + TLV_LENGTH_LENGTH + len > dynar_size(tlv_iter->msg)) {
		return (-1);
	}

	return (1);
}

int
tlv_iter_decode_u32(struct tlv_iterator *tlv_iter, uint32_t *res)
{
	const char *opt_data;
	uint16_t opt_len;
	uint32_t nu32;

	opt_len = tlv_iter_get_len(tlv_iter);
	opt_data = tlv_iter_get_data(tlv_iter);

	if (opt_len != sizeof(nu32)) {
		return (-1);
	}

	memcpy(&nu32, opt_data, sizeof(nu32));
	*res = ntohl(nu32);

	return (0);
}

int
tlv_iter_decode_u8(struct tlv_iterator *tlv_iter, uint8_t *res)
{
	const char *opt_data;
	uint16_t opt_len;

	opt_len = tlv_iter_get_len(tlv_iter);
	opt_data = tlv_iter_get_data(tlv_iter);

	if (opt_len != sizeof(*res)) {
		return (-1);
	}

	memcpy(res, opt_data, sizeof(*res));

	return (0);
}

int
tlv_iter_decode_str(struct tlv_iterator *tlv_iter, char **str, size_t *str_len)
{
	const char *opt_data;
	uint16_t opt_len;
	char *tmp_str;

	opt_len = tlv_iter_get_len(tlv_iter);
	opt_data = tlv_iter_get_data(tlv_iter);

	tmp_str = malloc(opt_len + 1);
	if (tmp_str == NULL) {
		return (-1);
	}

	memcpy(tmp_str, opt_data, opt_len);
	tmp_str[opt_len] = '\0';

	*str = tmp_str;
	*str_len = opt_len;

	return (0);
}
