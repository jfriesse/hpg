#include <sys/types.h>
#include <arpa/inet.h>

#include <inttypes.h>
#include <string.h>

#include "tlv.h"

int
tlv_add(char *msg, size_t msg_len, size_t *pos, enum tlv_opt_type opt_type, uint16_t opt_len,
	const void *value)
{
	uint16_t nlen;
	uint16_t nopt_type;

	if (*pos + sizeof(nopt_type) + sizeof(nlen) + opt_len > msg_len) {
		return (-1);
	}

	nopt_type = htons((uint16_t)opt_type);
	memcpy(msg + *pos, &nopt_type, sizeof(nopt_type));
	*pos += sizeof(nopt_type);

	nlen = htons(opt_len);
	memcpy(msg + *pos, &opt_len, sizeof(opt_len));
	*pos += sizeof(opt_len);

	memcpy(msg + *pos, value, opt_len);
	*pos += opt_len;

	return (0);
}

int
tlv_add_u32(char *msg, size_t msg_len, size_t *pos, enum tlv_opt_type opt_type, uint32_t u32)
{
	uint32_t nu32;

	nu32 = htonl(u32);

	return (tlv_add(msg, msg_len, pos, opt_type, sizeof(nu32), &nu32));
}

int
tlv_add_string(char *msg, size_t msg_len, size_t *pos, enum tlv_opt_type opt_type, const char *str)
{
	return (tlv_add(msg, msg_len, pos, opt_type, strlen(str), str));
}

int
tlv_add_msg_seq_number(char *msg, size_t msg_len, size_t *pos, uint32_t msg_seq_number)
{
	return (tlv_add_u32(msg, msg_len, pos, TLV_OPT_MSG_SEQ_NUMBER, msg_seq_number));
}

int
tlv_add_cluster_name(char *msg, size_t msg_len, size_t *pos, const char *cluster_name)
{
	return (tlv_add_string(msg, msg_len, pos, TLV_OPT_CLUSTER_NAME, cluster_name));
}
