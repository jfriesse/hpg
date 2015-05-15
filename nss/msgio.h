#ifndef _MSGIO_H_
#define _MSGIO_H_

#include <nspr.h>

#include "dynar.h"

#ifdef __cplusplus
extern "C" {
#endif

extern ssize_t	msgio_send(PRFileDesc *socket, const char *msg, size_t msg_len,
    size_t *start_pos);

extern ssize_t	msgio_send_blocking(PRFileDesc *socket, const char *msg, size_t msg_len);

extern int	msgio_write(PRFileDesc *socket, const struct dynar *msg, size_t *already_sent_bytes);

extern int	msgio_read(PRFileDesc *socket, struct dynar *msg, size_t *already_received_bytes, int *skipping_msg);

#ifdef __cplusplus
}
#endif

#endif /* _MSGIO_H_ */
