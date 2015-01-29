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
	if (PR_SetSocketOption(sock, &sock_opt) != SECSuccess) {
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
		if (PR_SetSocketOption(socket, &socket_option) != SECSuccess) {
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

		if (PR_InitializeNetAddr(PR_IpAddrAny, port, &addr) != SECSuccess) {
			return (NULL);
		}
		addr.raw.family = af;

		socket = nss_sock_create_socket(af, 1);
		if (socket == NULL) {
			return (NULL);
		}

		if (PR_Bind(socket, &addr) != SECSuccess) {
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

				if (PR_Bind(socket, &addr) != SECSuccess) {
					PR_Close(socket);
					socket = NULL;

					continue ;
				}

				/*
				 * Socket is sucesfully bound
				 */
				break ;
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

		if ((res = PR_Connect(socket, &addr, timeout)) != SECSuccess) {
			PR_Close(socket);
			socket = NULL;
			connect_failed = 1;
		}

		/*
		 * Connection attempt finished
		 */
		break ;
	}

	PR_FreeAddrInfo(addr_info);

	if (socket == NULL && !connect_failed) {
		PR_SetError(PR_ADDRESS_NOT_AVAILABLE_ERROR, 0);
	}

	return (socket);
}