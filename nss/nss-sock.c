#include "nss-sock.h"

#include <nss.h>
#include <ssl.h>

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
