#include "msgio.h"

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
