#ifndef _QNETD_CLIENT_H_
#define _QNETD_CLIENT_H_

#include <sys/types.h>
#include <inttypes.h>

#include <nspr.h>

#ifdef __cplusplus
extern "C" {
#endif

struct qnetd_client {
	PRFileDesc *socket;
	unsigned int test;
};

#ifdef __cplusplus
}
#endif

#endif /* _QNETD_CLIENT_H_ */
