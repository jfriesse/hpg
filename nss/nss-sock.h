#ifndef _NSS_SOCK_H_
#define _NSS_SOCK_H_

#include <nss.h>
#include <ssl.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int		nss_sock_init_nss(char *config_dir);
extern PRFileDesc	*nss_sock_create_listen_socket(const char *hostname, uint16_t port, PRIntn af);
extern int		nss_sock_set_nonblocking(PRFileDesc *sock);
extern PRFileDesc 	*nss_sock_create_client_socket(const char *hostname, uint16_t port, PRIntn af, PRIntervalTime timeout);

extern PRFileDesc	*nss_sock_start_ssl_as_client(PRFileDesc *input_sock, const char *ssl_url,
    SSLBadCertHandler bad_cert_hook);

extern PRFileDesc	*nss_sock_start_ssl_as_server(PRFileDesc *input_sock, CERTCertificate *server_cert,
    SECKEYPrivateKey *server_key);

#ifdef __cplusplus
}
#endif

#endif /* _NSS_SOCK_H_ */
