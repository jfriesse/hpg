#include <sys/types.h>
#include <arpa/inet.h>

#include <inttypes.h>
#include <string.h>

#include "msg.h"
#include "tlv.h"

#define MSG_TYPE_LENGTH		2
#define MSG_LENGTH_LENGTH	4

static void
msg_add_type(struct dynar *msg, enum msg_type type)
{
	uint16_t ntype;

	ntype = htons((uint16_t)type);
	dynar_cat(msg, &ntype, sizeof(ntype));
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
