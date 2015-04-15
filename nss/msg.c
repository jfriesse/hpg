#include <sys/types.h>
#include <arpa/inet.h>

#include <inttypes.h>
#include <string.h>

#include "msg.h"
#include "tlv.h"

#define MSG_TYPE_LENGTH		2
#define MSG_LENGTH_LENGTH	4

static void
msg_set_type(char *msg, enum msg_type type)
{
	uint16_t ntype;

	ntype = htons((uint16_t)type);
	memcpy(msg, &ntype, sizeof(ntype));
}

static void
msg_set_len(char *msg, uint32_t len)
{
	uint32_t nlen;

	nlen = htonl(len);
	memcpy(msg + 2, &nlen, sizeof(nlen));
}

size_t
msg_create_preinit(char *msg, size_t msg_len, const char *cluster_name, uint32_t msg_seq_number)
{
	size_t pos;

	msg_set_type(msg, MSG_TYPE_PREINIT);

	pos = MSG_TYPE_LENGTH + MSG_LENGTH_LENGTH;

	if (tlv_add_msg_seq_number(msg, msg_len, &pos, msg_seq_number) == -1) {
		goto small_buf_err;
	}

	if (tlv_add_cluster_name(msg, msg_len, &pos, cluster_name) == -1) {
		goto small_buf_err;
	}

	msg_set_len(msg, pos - (MSG_TYPE_LENGTH + MSG_LENGTH_LENGTH));

	return (pos);

small_buf_err:
	return (0);
}
