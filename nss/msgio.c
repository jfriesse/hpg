#include "msgio.h"
#include "msg.h"

#define MSGIO_LOCAL_BUF_SIZE			(1 << 10)

ssize_t
msgio_send(PRFileDesc *socket, const char *msg, size_t msg_len, size_t *start_pos)
{
	ssize_t sent_bytes;

	if ((sent_bytes = PR_Send(socket, msg + *start_pos,
	    msg_len - *start_pos, 0, PR_INTERVAL_NO_TIMEOUT)) != -1) {
		*start_pos += sent_bytes;
	}

	return (sent_bytes);
}

ssize_t
msgio_send_blocking(PRFileDesc *socket, const char *msg, size_t msg_len)
{
	PRPollDesc pfd;
	size_t already_sent_bytes;
	PRInt32 res;
	ssize_t ret;

	already_sent_bytes = 0;
	ret = 0;

	while (ret != -1 && already_sent_bytes < msg_len) {
		pfd.fd = socket;
		pfd.in_flags = PR_POLL_WRITE;
		pfd.out_flags = 0;

		if ((res = PR_Poll(&pfd, 1, PR_INTERVAL_NO_TIMEOUT)) > 0) {
			if (pfd.out_flags & PR_POLL_WRITE) {
				if ((msgio_send(socket, msg, msg_len, &already_sent_bytes) == -1) &&
				    PR_GetError() != PR_WOULD_BLOCK_ERROR) {
					ret = -1;
				} else {
					ret = already_sent_bytes;
				}
			} else if (pfd.out_flags & (PR_POLL_ERR | PR_POLL_NVAL | PR_POLL_HUP)) {
				PR_SetError(PR_IO_ERROR, 0);
				ret = -1;
			}
		} else {
			ret = -1;
		}
	}

	return (ret);
}

/*
 * -1 means send returned 0, -2  unhandled error. 0 = success but whole buffer is still not sent, 1 = all data was sent
 */
int
msgio_write(PRFileDesc *socket, const struct dynar *msg, size_t *already_sent_bytes)
{
	PRInt32 sent;
	PRInt32 to_send;

	to_send = dynar_size(msg) - *already_sent_bytes;
	if (to_send > MSGIO_LOCAL_BUF_SIZE) {
		to_send = MSGIO_LOCAL_BUF_SIZE;
	}

	sent = PR_Send(socket, dynar_data(msg) + *already_sent_bytes, to_send, 0, PR_INTERVAL_NO_TIMEOUT);

	if (sent > 0) {
		*already_sent_bytes += sent;

		if (*already_sent_bytes == dynar_size(msg)) {
			/*
			 * All data sent
			 */
			return (1);
		}
	}

	if (sent == 0) {
		return (-1);
	}

	if (sent < 0 && PR_GetError() != PR_WOULD_BLOCK_ERROR) {
		return (-2);
	}

	return (0);
}

/*
 * -1 End of connection
 * -2 Unhandled error
 * -3 Fatal error. Unable to store message header
 * -4 Unable to store message
 * -5 Invalid msg type
 * -6 Msg too long
 */
int
msgio_read(PRFileDesc *socket, struct dynar *msg, size_t *already_received_bytes, int *skipping_msg)
{
	char local_read_buffer[MSGIO_LOCAL_BUF_SIZE];
	PRInt32 readed;
	PRInt32 to_read;
	int ret;

	ret = 0;

	if (*already_received_bytes < msg_get_header_length()) {
		/*
		 * Complete reading of header
		 */
		to_read = msg_get_header_length() - *already_received_bytes;
	} else {
		/*
		 * Read rest of message (or at least as much as possible)
		 */
		to_read = (msg_get_header_length() + msg_get_len(msg)) - *already_received_bytes;
	}

	if (to_read > MSGIO_LOCAL_BUF_SIZE) {
		to_read = MSGIO_LOCAL_BUF_SIZE;
	}

	readed = PR_Recv(socket, local_read_buffer, to_read, 0, PR_INTERVAL_NO_TIMEOUT);
	if (readed > 0) {
		*already_received_bytes += readed;

		if (!*skipping_msg) {
			if (dynar_cat(msg, local_read_buffer, readed) == -1) {
				*skipping_msg = 1;
				ret = -4;
			}
		}

		if (*skipping_msg && *already_received_bytes < msg_get_header_length()) {
			/*
			 * Fatal error. We were unable to store even message header
			 */
			return (-3);
		}

		if (!*skipping_msg && *already_received_bytes == msg_get_header_length()) {
			/*
			 * Full header received. Check type, maximum size, ...
			 */
			if (!msg_is_valid_msg_type(msg)) {
				*skipping_msg = 1;
				ret = -5;
			} else if (msg_get_header_length() + msg_get_len(msg) > dynar_max_size(msg)) {
				*skipping_msg = 1;
				ret = -6;
			}
		}

		if (*already_received_bytes >= msg_get_header_length() &&
		    *already_received_bytes == (msg_get_header_length() + msg_get_len(msg))) {
			/*
			 * Full message skipped or received
			 */
			ret = 1;
		}

	}

	if (readed == 0) {
		return (-1);
	}

	if (readed < 0 && PR_GetError() != PR_WOULD_BLOCK_ERROR) {
		return (-2);
	}

	return (ret);
}
