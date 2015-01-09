#include <stdio.h>
#include <nss.h>
#include <pk11func.h>
#include <certt.h>
#include <ssl.h>
#include <prio.h>
#include <prnetdb.h>
#include <prerror.h>
#include <prinit.h>
#include <getopt.h>
#include <err.h>
#include <keyhi.h>

#include "nss-sock.h"

#define NSS_DB_DIR	"nssdb"

static void err_nss(void) {
	errx(1, "nss error %d: %s", PR_GetError(), PR_ErrorToString(PR_GetError(), PR_LANGUAGE_I_DEFAULT));
}

static char *get_pwd(PK11SlotInfo *slot, PRBool retry, void *arg)
{
	FILE *f;
	char pwd[255];

	if (retry) {
		return (NULL);
	}

	f = fopen(NSS_DB_DIR"/pwdfile.txt", "rt");
	if (f == NULL) {
		err(1, "Can't open pwd file");
	}

	fgets(pwd, sizeof(pwd), f);
	fclose(f);

	if (pwd[strlen(pwd) - 1] == '\n')
		pwd[strlen(pwd) - 1] = '\0';

	fprintf(stderr, "Return %s password\n", pwd);

	return (PL_strdup(pwd));
}

PRFileDesc *
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

int main(void)
{
	PRFileDesc *socket;
	PRNetAddr client_addr;
	PRFileDesc* client_socket;
	CERTCertificate *server_cert;
	SECKEYPrivateKey *server_private_key;
	char buf[255];
	PRInt32 readed;

	if (nss_sock_init_nss(NSS_DB_DIR) != 0) {
		err_nss();
	}


	/*
	 * Socket is ready, now find addr to bind to
	 */
//	addr_info = PR_GetAddrInfoByName("127.0.0.1", PR_AF_INET, PR_AI_ADDRCONFIG);
/*	addr_info = PR_GetAddrInfoByName("192.168.1.1", PR_AF_UNSPEC, PR_AI_ADDRCONFIG);
	if (addr_info == NULL) {
		err_nss();
	}

	addr_iter = NULL;
	addr_bound = 0;

	while ((addr_iter = PR_EnumerateAddrInfo(addr_iter, addr_info, 4433, &addr)) != NULL) {


		if (PR_Bind(socket, &addr) == PR_SUCCESS) {
			addr_bound = 1;
			break;
		}
	}

	if (!addr_bound) {
		errx(1, "Can't bound to address");
	}*/
	socket = nss_sock_create_listen_socket(NULL, 4433, PR_AF_INET6);
	if (socket == NULL) {
		err_nss();
	}

	if (PR_Listen(socket, 10) != PR_SUCCESS) {
		err_nss();
	}

	if ((client_socket = PR_Accept(socket, &client_addr, PR_INTERVAL_NO_TIMEOUT)) == NULL) {
		err_nss();
	}

	PK11_SetPasswordFunc(get_pwd);

	server_cert =  PK11_FindCertFromNickname("QNetd Cert", NULL);
	if (!server_cert) {
		err_nss();
	}

	server_private_key = PK11_FindKeyByAnyCert(server_cert, NULL);
	if (server_private_key == NULL) {
		err_nss();
	}

	client_socket = SSL_ImportFD(NULL, client_socket);
	if (client_socket == NULL) {
		err_nss();
	}

	if (SSL_ConfigSecureServer(client_socket, server_cert, server_private_key, NSS_FindCertKEAType(server_cert)) != PR_SUCCESS) {
		err_nss();
	}

	if ((SSL_OptionSet(client_socket, SSL_SECURITY, PR_TRUE) != SECSuccess) ||
	    (SSL_OptionSet(client_socket, SSL_HANDSHAKE_AS_SERVER, PR_TRUE) != SECSuccess) ||
	    (SSL_OptionSet(client_socket, SSL_HANDSHAKE_AS_CLIENT, PR_FALSE) != SECSuccess)) {
		err_nss();
	}

	if (SSL_ResetHandshake(client_socket, PR_TRUE) != SECSuccess) {
		err_nss();
	}


	while ((readed = PR_Read(client_socket, buf, sizeof(buf))) > 0) {
		buf[readed] = '\0';
		printf("Readed %u bytes: %s\n", readed, buf);
	}

	if (readed < 0) {
		err_nss();
	}

	PR_Close(client_socket);
	PR_Close(socket);
	CERT_DestroyCertificate(server_cert);
	SECKEY_DestroyPrivateKey(server_private_key);

	if (NSS_Shutdown() != SECSuccess) {
		err_nss();
	}

	PR_Cleanup();

	return (0);
}
