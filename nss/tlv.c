#include <sys/types.h>
#include <arpa/inet.h>

#include <inttypes.h>
#include <string.h>

#include "tlv.h"

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
