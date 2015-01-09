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
