#include <sys/types.h>
#include <arpa/inet.h>

#include <inttypes.h>
#include <string.h>

#include "msg.h"
#include "tlv.h"

#define MSG_TYPE_LENGTH		2
#define MSG_LENGTH_LENGTH	4

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
msg_create_preinit(struct dynar *msg, const char *cluster_name, uint32_t msg_seq_number)
{

	dynar_clean(msg);

	msg_add_type(msg, MSG_TYPE_PREINIT);
	msg_add_len(msg);

	if (tlv_add_msg_seq_number(msg, msg_seq_number) == -1) {
		goto small_buf_err;
	}

	if (tlv_add_cluster_name(msg, cluster_name) == -1) {
		goto small_buf_err;
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
		res = 1;
		break;
	default:
		res = 0;
		break;
	}

	return (res);
}
