/*
 * Copyright (c) 2015-2016 Red Hat, Inc.
 *
 * All rights reserved.
 *
 * Author: Jan Friesse (jfriesse@redhat.com)
 *
 * This software licensed under BSD license, the text of which follows:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name of the Red Hat, Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <config.h>

#include <cmap.h>
#include <votequorum.h>
#include <err.h>
#include "qb/qblog.h"

#include "qdevice-config.h"
#include "qdevice-cmap.h"
#include "utils.h"

#define qdevice_log	qb_log

struct qdevice_log_syslog_names {
	const char *prio_name;
	int priority;
};

static struct qdevice_log_syslog_names qdevice_log_priority_names[] = {
	{ "alert", LOG_ALERT },
	{ "crit", LOG_CRIT },
	{ "debug", LOG_DEBUG },
	{ "emerg", LOG_EMERG },
	{ "err", LOG_ERR },
	{ "error", LOG_ERR },
	{ "info", LOG_INFO },
	{ "notice", LOG_NOTICE },
	{ "warning", LOG_WARNING },
	{ NULL, -1 }};

static int
qdevice_log_priority_str_to_int(const char *priority_str)
{
	unsigned int i;

	for (i = 0; qdevice_log_priority_names[i].prio_name != NULL; i++) {
		if (strcasecmp(priority_str, qdevice_log_priority_names[i].prio_name) == 0) {
			return (qdevice_log_priority_names[i].priority);
		}
	}

	return (-1);
}

static int
qdevice_log_configure(struct qdevice_instance *instance)
{
	char qb_error_str[128];
	int qb_err;
	int to_stderr;
	int to_syslog;
	int to_logfile;
	int syslog_facility;
	int syslog_priority;
	char *str;
	char *logfile_name;
	int reconfigure_logfile;
	int i;

	if (cmap_get_string(instance->cmap_handle, "logging.to_stderr", &str) != CS_OK) {
		to_stderr = QDEVICE_LOG_DEFAULT_TO_STDERR;
	} else {
		if ((i = utils_parse_bool_str(str)) == -1) {
			qdevice_log(LOG_WARNING, "logging.to_stderr value is not valid");
		} else {
			to_stderr = i;
		}
		free(str);
	}

	if (cmap_get_string(instance->cmap_handle,
	    "logging.logger_subsys." QDEVICE_LOG_SUBSYS ".to_stderr", &str) == CS_OK) {
		if ((i = utils_parse_bool_str(str)) == -1) {
			qdevice_log(LOG_WARNING,
			    "logging.logger_subsys." QDEVICE_LOG_SUBSYS ".to_stderr value is not valid.");
		} else {
			to_stderr = i;
		}
		free(str);
	}

	if (cmap_get_string(instance->cmap_handle, "logging.to_syslog", &str) != CS_OK) {
		to_syslog = QDEVICE_LOG_DEFAULT_TO_SYSLOG;
	} else {
		if ((i = utils_parse_bool_str(str)) == -1) {
			qdevice_log(LOG_WARNING, "logging.to_syslog value is not valid");
		} else {
			to_syslog = i;
		}
		free(str);
	}

	if (cmap_get_string(instance->cmap_handle,
	    "logging.logger_subsys." QDEVICE_LOG_SUBSYS ".to_syslog", &str) == CS_OK) {
		if ((i = utils_parse_bool_str(str)) == -1) {
			qdevice_log(LOG_WARNING,
			    "logging.logger_subsys." QDEVICE_LOG_SUBSYS ".to_syslog value is not valid.");
		} else {
			to_syslog = i;
		}
		free(str);
	}

	if (cmap_get_string(instance->cmap_handle, "logging.to_logfile", &str) != CS_OK) {
		to_logfile = QDEVICE_LOG_DEFAULT_TO_LOGFILE;
	} else {
		if ((i = utils_parse_bool_str(str)) == -1) {
			qdevice_log(LOG_WARNING, "logging.to_logfile value is not valid");
		} else {
			to_logfile = i;
		}
		free(str);
	}

	if (cmap_get_string(instance->cmap_handle,
	    "logging.logger_subsys." QDEVICE_LOG_SUBSYS ".to_logfile", &str) == CS_OK) {
		if ((i = utils_parse_bool_str(str)) == -1) {
			qdevice_log(LOG_WARNING,
			    "logging.logger_subsys." QDEVICE_LOG_SUBSYS ".to_logfile value is not valid.");
		} else {
			to_logfile = i;
		}
		free(str);
	}

	if (cmap_get_string(instance->cmap_handle, "logging.syslog_facility", &str) != CS_OK) {
		syslog_facility = QDEVICE_LOG_DEFAULT_SYSLOG_FACILITY;
	} else {
		if ((i = qb_log_facility2int(str)) < 0) {
			qdevice_log(LOG_WARNING, "logging.syslog_facility value is not valid");
		} else {
			syslog_facility = i;
		}

		free(str);
	}

	if (cmap_get_string(instance->cmap_handle,
	    "logging.logger_subsys." QDEVICE_LOG_SUBSYS ".syslog_facility", &str) == CS_OK) {
		if ((i = qb_log_facility2int(str)) < 0) {
			qdevice_log(LOG_WARNING,
			    "logging.logger_subsys." QDEVICE_LOG_SUBSYS ".syslog_facility value is not valid.");
		} else {
			syslog_facility = i;
		}
		free(str);
	}

	if (cmap_get_string(instance->cmap_handle, "logging.syslog_priority", &str) != CS_OK) {
		syslog_priority = QDEVICE_LOG_DEFAULT_SYSLOG_PRIORITY;
	} else {
		if ((i = qdevice_log_priority_str_to_int(str)) < 0) {
			qdevice_log(LOG_WARNING, "logging.syslog_priority value is not valid");
		} else {
			syslog_priority = i;
		}

		free(str);
	}

	if (cmap_get_string(instance->cmap_handle,
	    "logging.logger_subsys." QDEVICE_LOG_SUBSYS ".syslog_priority", &str) == CS_OK) {
		if ((i = qdevice_log_priority_str_to_int(str)) < 0) {
			qdevice_log(LOG_WARNING,
			    "logging.logger_subsys." QDEVICE_LOG_SUBSYS ".syslog_priority value is not valid.");
		} else {
			syslog_priority = i;
		}
		free(str);
	}

	if (cmap_get_string(instance->cmap_handle,
	    "logging.logger_subsys." QDEVICE_LOG_SUBSYS ".logfile", &str) == CS_OK) {
		logfile_name = str;
		if (strcmp(logfile_name, "") == 0) {
			free(logfile_name);
			logfile_name = NULL;
		}
	} else {
		logfile_name = NULL;
	}

	/*
	 * Finally reconfigure log system
	 */
	qb_log_ctl(QB_LOG_STDERR, QB_LOG_CONF_ENABLED, to_stderr);
	qb_log_ctl(QB_LOG_SYSLOG, QB_LOG_CONF_ENABLED, to_syslog);
	qb_log_ctl(QB_LOG_SYSLOG, QB_LOG_CONF_FACILITY, syslog_facility);
	qb_log_filter_ctl(QB_LOG_SYSLOG, QB_LOG_FILTER_CLEAR_ALL, QB_LOG_FILTER_FILE, "*", LOG_TRACE);
	qb_log_filter_ctl(QB_LOG_SYSLOG, QB_LOG_FILTER_ADD, QB_LOG_FILTER_FILE, "*", syslog_priority);

	reconfigure_logfile = 0;

	if (instance->qdevice_log_logfile_name != NULL) {
		/*
		 * Logfile is in use
		 */
		if (logfile_name == NULL || !to_logfile) {
			/*
			 * New config ether disables to_logfile or doesn't have valid logfile_name
			 */
			reconfigure_logfile = 1;
		} else {
			/*
			 * Reconfigure only if new and old fname differs or to_logfile is disabled
			 */
			reconfigure_logfile =
			    ((strcmp(instance->qdevice_log_logfile_name, logfile_name) != 0) ||
			    !to_logfile);
		}
	} else {
		/*
		 * Logfile is not in use. Reconfig needed only when new logfile_name is not empty and
		 * to_logfile is set
		 */
		if (logfile_name != NULL && to_logfile) {
			reconfigure_logfile = 1;
		}
	}

	if (reconfigure_logfile) {
		if (instance->qdevice_log_logfile_name != NULL) {
			qb_log_file_close(instance->qdevice_log_logfile_id);
		}

		if (to_logfile && logfile_name != NULL) {
			instance->qdevice_log_logfile_id = qb_log_file_open(logfile_name);
			if (instance->qdevice_log_logfile_id < 0) {
				qb_err = -instance->qdevice_log_logfile_id;

				qdevice_log(LOG_WARNING, "Can't open logging file %s for reason %s (%d)",
				    logfile_name, qb_strerror_r(qb_err, qb_error_str, sizeof(qb_error_str)), err);
			} else {
				qb_log_ctl(instance->qdevice_log_logfile_id, QB_LOG_CONF_ENABLED, QB_TRUE);
				qb_log_filter_ctl(instance->qdevice_log_logfile_id,
				    QB_LOG_FILTER_ADD, QB_LOG_FILTER_FILE, "*", LOG_DEBUG);

				instance->qdevice_log_logfile_name = strdup(logfile_name);
				if (instance->qdevice_log_logfile_name == NULL) {
					qdevice_log(LOG_CRIT, "Can't alloc logfile_name memory");
					exit(1);
				}
			}
		}
	}

	free(logfile_name);
	printf("A %u %u %u\n", to_stderr, to_syslog, syslog_facility);
	return (0);
}

static void
qdevice_log_init(struct qdevice_instance *instance)
{
	qb_log_init(QDEVICE_LOG_NAME, QDEVICE_LOG_DEFAULT_SYSLOG_FACILITY,
	    QDEVICE_LOG_DEFAULT_SYSLOG_PRIORITY);

	qb_log_ctl(QB_LOG_SYSLOG, QB_LOG_CONF_ENABLED, QB_FALSE);
	qb_log_ctl(QB_LOG_STDOUT, QB_LOG_CONF_ENABLED, QB_FALSE);
	qb_log_ctl(QB_LOG_BLACKBOX, QB_LOG_CONF_ENABLED, QB_FALSE);
	qb_log_ctl(QB_LOG_STDERR, QB_LOG_CONF_ENABLED, QB_TRUE);

	qb_log_filter_ctl(QB_LOG_STDERR, QB_LOG_FILTER_ADD, QB_LOG_FILTER_FILE, "*", LOG_DEBUG);
	qb_log_filter_ctl(QB_LOG_SYSLOG, QB_LOG_FILTER_ADD, QB_LOG_FILTER_FILE, "*", LOG_INFO);
	qb_log_format_set(QB_LOG_STDERR, "%t %-7p %b");

	qdevice_log_configure(instance);
	qdevice_log(LOG_ERR, "Ahoj");
	qdevice_log(LOG_DEBUG, "Ahojdebug");
	qb_log_fini();
}

int
main(void)
{
	struct qdevice_instance instance;

	qdevice_instance_init(&instance);
	qdevice_cmap_init(&instance);
	qdevice_log_init(&instance);

	qdevice_cmap_destroy(&instance);

/*	cmap_handle_t cmap_handle;
	struct send_buffer_list_entry *send_buffer;
	int try_connect;*/

	/*
	 * Init
	 */
/*	qdevice_net_cmap_init(&cmap_handle);
	qdevice_net_instance_init_from_cmap(&instance, cmap_handle);

	qdevice_net_log_init(QDEVICE_NET_LOG_TARGET_STDERR);
        qdevice_net_log_set_debug(1);

	qdevice_net_log(LOG_DEBUG, "Registering algorithms");
	qdevice_net_algorithm_register_all();

	if (nss_sock_init_nss((instance.tls_supported != TLV_TLS_UNSUPPORTED ?
	    (char *)QDEVICE_NET_NSS_DB_DIR : NULL)) != 0) {
		qdevice_net_log_nss(LOG_ERR, "Can't init nss");
		exit(1);
	}


	if (qdevice_net_algorithm_init(&instance) != 0) {
		qdevice_net_log(LOG_ERR, "Algorithm init failed");
		exit(1);
	}

	try_connect = 1;
	while (try_connect) {
		qdevice_net_votequorum_init(&instance);*/

		/*
		 * Try to connect to qnetd host
		 */
/*		instance.socket = nss_sock_create_client_socket(instance.host_addr, instance.host_port,
		    PR_AF_UNSPEC, PR_MillisecondsToInterval(instance.connect_timeout));
		if (instance.socket == NULL) {
			qdevice_net_log_nss(LOG_CRIT, "Can't connect to server");
			poll(NULL, 0, QDEVICE_NET_PAUSE_BEFORE_RECONNECT);

			continue ;
		}

		if (nss_sock_set_nonblocking(instance.socket) != 0) {
			if (PR_Close(instance.socket) != PR_SUCCESS) {
				qdevice_net_log_nss(LOG_WARNING, "Unable to close connection");
			}

			qdevice_net_log_nss(LOG_CRIT, "Can't set socket nonblocking");
			poll(NULL, 0, QDEVICE_NET_PAUSE_BEFORE_RECONNECT);

			continue ;
		}
*/
		/*
		 * Create and schedule send of preinit message to qnetd
		 */
/*		send_buffer = send_buffer_list_get_new(&instance.send_buffer_list);
		if (send_buffer == NULL) {
			errx(1, "Can't allocate send buffer list");
		}

		if (msg_create_preinit(&send_buffer->buffer, instance.cluster_name, 1,
		    instance.last_msg_seq_num) == 0) {
			errx(1, "Can't allocate buffer");
		}

		send_buffer_list_put(&instance.send_buffer_list, send_buffer);

		instance.state = QDEVICE_NET_INSTANCE_STATE_WAITING_PREINIT_REPLY;
*/
		/*
		 * Main loop
		 */
/*		while (qdevice_net_poll(&instance) == 0) {
		}

		try_connect = qdevice_net_disconnect_reason_try_reconnect(instance.disconnect_reason);

		if (qdevice_net_algorithm_disconnected(&instance, instance.disconnect_reason,
		    &try_connect) != 0) {
			qdevice_net_log(LOG_ERR, "Algorithm returned error, force exit");
			exit(2);
		}

		if (instance.disconnect_reason == QDEVICE_NET_DISCONNECT_REASON_COROSYNC_CONNECTION_CLOSED) {
			try_connect = 0;
		}
*/
		/*
		 * Cleanup
		 */
/*		if (PR_Close(instance.socket) != PR_SUCCESS) {
			qdevice_net_log_nss(LOG_WARNING, "Unable to close connection");
		}
*/
		/*
		 * Close cmap and votequorum connections
		 */
/*		qdevice_net_votequorum_destroy(&instance);
		qdevice_net_cmap_del_track(&instance);

		if (try_connect) {*/
			/*
			 * Reinit instance
			 */
/*			qdevice_net_instance_clean(&instance);
		}
	}

	qdevice_net_algorithm_destroy(&instance);

	qdevice_net_cmap_destroy(&instance);

	qdevice_net_instance_destroy(&instance);

	SSL_ClearSessionCache();

	if (NSS_Shutdown() != SECSuccess) {
		qdevice_net_log_nss(LOG_WARNING, "Can't shutdown NSS");
	}

	PR_Cleanup();

	qdevice_net_log_close();*/

	return (0);
}
