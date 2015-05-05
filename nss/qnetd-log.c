#include <syslog.h>
#include <stdarg.h>
#include <stdio.h>

#include "qnetd-defines.h"
#include "qnetd-log.h"

static int qnetd_log_config_target = 0;
static int qnetd_log_config_debug = 0;

void
qnetd_log_init(int target)
{

	qnetd_log_config_target = target;

	if (qnetd_log_config_target & QNETD_LOG_TARGET_SYSLOG) {
		openlog(QNETD_PROGRAM_NAME, LOG_PID, LOG_DAEMON);
	}
}

void
qnetd_log_printf(int priority, const char *format, ...)
{
	va_list ap;

	if (priority != LOG_DEBUG || (qnetd_log_config_debug)) {
		if (qnetd_log_config_target & QNETD_LOG_TARGET_STDERR) {
			va_start(ap, format);
			vfprintf(stderr, format, ap);
			fprintf(stderr, "\n");
			va_end(ap);
		}

		if (qnetd_log_config_target & QNETD_LOG_TARGET_SYSLOG) {
			va_start(ap, format);
			vsyslog(priority, format, ap);
			va_end(ap);
		}
	}
}

void
qnetd_log_close(void)
{

	if (qnetd_log_config_target & QNETD_LOG_TARGET_SYSLOG) {
		closelog();
	}
}

void
qnetd_log_set_debug(int enabled)
{

	qnetd_log_config_debug = enabled;
}
