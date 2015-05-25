#include <prnetdb.h>

#include "nss-sock.h"

int
nss_sock_init_nss(char *config_dir)
{
	if (NSS_Init(config_dir) != SECSuccess) {
		return (-1);
	}

	if (NSS_SetDomesticPolicy() != SECSuccess) {
		return (-1);
	}

	return (0);
}

/*
 * Set NSS socket non-blocking
 */
int
nss_sock_set_nonblocking(PRFileDesc *sock)
{
	PRSocketOptionData sock_opt;

	memset(&sock_opt, 0, sizeof(sock_opt));
	sock_opt.option = PR_SockOpt_Nonblocking;
	sock_opt.value.non_blocking = PR_TRUE;
	if (PR_SetSocketOption(sock, &sock_opt) != PR_SUCCESS) {
		return (-1);
	}

	return (0);
}

/*
 * Create TCP socket with af family. If reuse_addr is set, socket option
 * for reuse address is set.
 */
static PRFileDesc *
nss_sock_create_socket(PRIntn af, int reuse_addr)
{
	PRFileDesc *socket;
	PRSocketOptionData socket_option;

	socket = PR_OpenTCPSocket(af);
	if (socket == NULL) {
		return (NULL);
	}

	if (reuse_addr) {
		socket_option.option = PR_SockOpt_Reuseaddr;
		socket_option.value.reuse_addr = PR_TRUE;
		if (PR_SetSocketOption(socket, &socket_option) != PR_SUCCESS) {
			return (NULL);
	         }
	}

	return (socket);
}

/*
 * Create listen socket and bind it to address. hostname can be NULL and then
 * any address is used. Address family (af) can be ether PR_AF_INET6 or
 * PR_AF_INET.
 */
PRFileDesc *
nss_sock_create_listen_socket(const char *hostname, uint16_t port, PRIntn af)
{
	PRNetAddr addr;
	PRFileDesc *socket;
	PRAddrInfo *addr_info;
	PRIntn tmp_af;
	void *addr_iter;

	socket = NULL;

	if (hostname == NULL) {
		memset(&addr, 0, sizeof(addr));

		if (PR_InitializeNetAddr(PR_IpAddrAny, port, &addr) != PR_SUCCESS) {
			return (NULL);
		}
		addr.raw.family = af;

		socket = nss_sock_create_socket(af, 1);
		if (socket == NULL) {
			return (NULL);
		}

		if (PR_Bind(socket, &addr) != PR_SUCCESS) {
			PR_Close(socket);

			return (NULL);
		}
	} else {
		tmp_af = PR_AF_UNSPEC;
		if (af == PR_AF_INET)
			tmp_af = PR_AF_INET;

		addr_info = PR_GetAddrInfoByName(hostname, tmp_af, PR_AI_ADDRCONFIG);
		if (addr_info == NULL) {
			return (NULL);
		}

		addr_iter = NULL;

		while ((addr_iter = PR_EnumerateAddrInfo(addr_iter, addr_info, port, &addr)) != NULL) {
			if (addr.raw.family == af) {
				socket = nss_sock_create_socket(af, 1);
				if (socket == NULL) {
					continue ;
				}

				if (PR_Bind(socket, &addr) != PR_SUCCESS) {
					PR_Close(socket);
					socket = NULL;

					continue ;
				}

				/*
				 * Socket is sucesfully bound
				 */
				break;
			}
		}

		PR_FreeAddrInfo(addr_info);

		if (socket == NULL) {
			/*
			 * No address succeeded
			 */
			PR_SetError(PR_ADDRESS_NOT_AVAILABLE_ERROR, 0);

			return (NULL);
		}
	}

	return (socket);
}

/*
 * Create listen socket and bind it to address. hostname can be NULL and then
 * any address is used. Address family (af) can be ether PR_AF_UNSPEC or
 * PR_AF_INET.
 */
PRFileDesc *
nss_sock_create_client_socket(const char *hostname, uint16_t port, PRIntn af, PRIntervalTime timeout)
{
	PRNetAddr addr;
	PRFileDesc *socket;
	PRAddrInfo *addr_info;
	void *addr_iter;
	PRStatus res;
	int connect_failed;

	socket = NULL;
	connect_failed = 0;

	addr_info = PR_GetAddrInfoByName(hostname, af, PR_AI_ADDRCONFIG);
	if (addr_info == NULL) {
		return (NULL);
	}

	addr_iter = NULL;

	while ((addr_iter = PR_EnumerateAddrInfo(addr_iter, addr_info, port, &addr)) != NULL) {
		socket = nss_sock_create_socket(addr.raw.family, 0);
		if (socket == NULL) {
			continue ;
		}

		if ((res = PR_Connect(socket, &addr, timeout)) != PR_SUCCESS) {
			PR_Close(socket);
			socket = NULL;
			connect_failed = 1;
		}

		/*
		 * Connection attempt finished
		 */
		break;
	}

	PR_FreeAddrInfo(addr_info);

	if (socket == NULL && !connect_failed) {
		PR_SetError(PR_ADDRESS_NOT_AVAILABLE_ERROR, 0);
	}

	return (socket);
}

/*
 * Start client side SSL connection. This can block.
 *
 * ssl_url is expected server URL, bad_cert_hook is callback called when server certificate
 * verification fails.
 */
PRFileDesc *
nss_sock_start_ssl_as_client(PRFileDesc *input_sock, const char *ssl_url, SSLBadCertHandler bad_cert_hook,
    SSLGetClientAuthData client_auth_hook, void *client_auth_hook_arg, int force_handshake, int *reset_would_block)
{
	PRFileDesc *ssl_sock;

	if (force_handshake) {
		*reset_would_block = 0;
	}

	ssl_sock = SSL_ImportFD(NULL, input_sock);
	if (ssl_sock == NULL) {
		return (NULL);
	}

	if (SSL_SetURL(ssl_sock, ssl_url) != SECSuccess) {
		return (NULL);
	}

	if ((SSL_OptionSet(ssl_sock, SSL_SECURITY, PR_TRUE) != SECSuccess) ||
	    (SSL_OptionSet(ssl_sock, SSL_HANDSHAKE_AS_SERVER, PR_FALSE) != SECSuccess) ||
	    (SSL_OptionSet(ssl_sock, SSL_HANDSHAKE_AS_CLIENT, PR_TRUE) != SECSuccess)) {
		return (NULL);
	}
	if (bad_cert_hook != NULL && SSL_BadCertHook(ssl_sock, bad_cert_hook, NULL) != SECSuccess) {
		return (NULL);
	}

	if (client_auth_hook != NULL &&
	    (SSL_GetClientAuthDataHook(ssl_sock, client_auth_hook, client_auth_hook_arg) != SECSuccess)) {
		return (NULL);
	}

	if (SSL_ResetHandshake(ssl_sock, PR_FALSE) != SECSuccess) {
		return (NULL);
	}

	if (force_handshake && SSL_ForceHandshake(ssl_sock) != SECSuccess) {
		if (PR_GetError() == PR_WOULD_BLOCK_ERROR) {
			/*
			 * Mask would block error.
			 */
			*reset_would_block = 1;
		} else {
	                return (NULL);
	        }
	}

	return (ssl_sock);
}

PRFileDesc *
nss_sock_start_ssl_as_server(PRFileDesc *input_sock, CERTCertificate *server_cert, SECKEYPrivateKey *server_key,
    int require_client_cert, int force_handshake, int *reset_would_block)
{
	PRFileDesc *ssl_sock;

	if (force_handshake) {
		*reset_would_block = 0;
	}

	ssl_sock = SSL_ImportFD(NULL, input_sock);
	if (ssl_sock == NULL) {
		return (NULL);
	}

	if (SSL_ConfigSecureServer(ssl_sock, server_cert, server_key, NSS_FindCertKEAType(server_cert)) != SECSuccess) {
		return (NULL);
	}

	if ((SSL_OptionSet(ssl_sock, SSL_SECURITY, PR_TRUE) != SECSuccess) ||
	    (SSL_OptionSet(ssl_sock, SSL_HANDSHAKE_AS_SERVER, PR_TRUE) != SECSuccess) ||
	    (SSL_OptionSet(ssl_sock, SSL_HANDSHAKE_AS_CLIENT, PR_FALSE) != SECSuccess) ||
	    (SSL_OptionSet(ssl_sock, SSL_REQUEST_CERTIFICATE, require_client_cert) != SECSuccess) ||
	    (SSL_OptionSet(ssl_sock, SSL_REQUIRE_CERTIFICATE, require_client_cert) != SECSuccess)) {
		return (NULL);
	}

	if (SSL_ResetHandshake(ssl_sock, PR_TRUE) != SECSuccess) {
		return (NULL);
	}

        if (force_handshake && SSL_ForceHandshake(ssl_sock) != SECSuccess) {
		if (PR_GetError() == PR_WOULD_BLOCK_ERROR) {
			/*
			 * Mask would block error.
			 */
			*reset_would_block = 1;
		} else {
	                return (NULL);
	        }
        }

	return (ssl_sock);
}
