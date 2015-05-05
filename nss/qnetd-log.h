#ifndef _QNETD_LOG_H_
#define _QNETD_LOG_H_

#ifdef __cplusplus
extern "C" {
#endif

#define QNETD_LOG_TARGET_STDERR		1
#define QNETD_LOG_TARGET_SYSLOG		2

#define qnetd_log(...)	qnetd_log_printf(__VA_ARGS__)
#define qnetd_log_nss(priority, str) qnetd_log_printf(priority, "%s (%d): %s", \
    str, PR_GetError(), PR_ErrorToString(PR_GetError(), PR_LANGUAGE_I_DEFAULT));

extern void		qnetd_log_init(int target);

extern void		qnetd_log_printf(int priority, const char *format, ...);

extern void		qnetd_log_close(void);

extern void		qnetd_log_set_debug(int enabled);

#ifdef __cplusplus
}
#endif

#endif /* _QNETD_LOG_H_ */
