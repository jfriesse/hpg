#ifndef _MSGIO_H_
#define _MSGIO_H_

#include <nspr.h>

#ifdef __cplusplus
extern "C" {
#endif

extern ssize_t	msgio_send(PRFileDesc *socket, const char *msg, size_t msg_len,
    size_t *start_pos);

extern ssize_t	msgio_send_blocking(PRFileDesc *socket, const char *msg, size_t msg_len);

#ifdef __cplusplus
}
#endif

#endif /* _MSGIO_H_ */