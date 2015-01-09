#ifndef _NSS_SOCK_H_
#define _NSS_SOCK_H_

#include <nss.h>
#include <ssl.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int		nss_sock_init_nss(char *config_dir);
extern PRFileDesc	*nss_sock_create_listen_socket(const char *hostname, uint16_t port, PRIntn af);

#ifdef __cplusplus
}
#endif

#endif /* _NSS_SOCK_H_ */
