#include <sys/types.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#define MAX_MSG_SIZE	512

static const char log_month_str[][4] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

void
usage(void)
{

	printf("usage: udptest [-s] ip_addr\n");
	exit(EXIT_FAILURE);
}

void
log_vprintf(int priority, int add_err, const char *format, va_list ap)
{
	struct timeval tv;
	struct tm tm_res;
	struct tm *localtime_res;
	int res;
	va_list ap_copy;
	int saved_errno;

	saved_errno = errno;

	res = gettimeofday(&tv, NULL);
	assert(res == 0);

	localtime_res = localtime_r(&tv.tv_sec, &tm_res);
	assert(localtime_res != NULL);

	fprintf(stderr, "%s %02d %02d:%02d:%02d.%03d ",
	    log_month_str[tm_res.tm_mon], tm_res.tm_mday, tm_res.tm_hour,
	    tm_res.tm_min, tm_res.tm_sec, tv.tv_usec / 1000);

	va_copy(ap_copy, ap);
	vfprintf(stderr, format, ap_copy);
	va_end(ap_copy);

	if (add_err) {
		fprintf(stderr, ": %s", strerror(saved_errno));
	}

	fprintf(stderr, "\n");
}

void
log_printf(int priority, const char *format, ...)
{
	va_list ap;

	va_start(ap, format);

	log_vprintf(priority, 0, format, ap);

	va_end(ap);
}

void
log_err(int priority, const char *format, ...)
{
	va_list ap;

	va_start(ap, format);

	log_vprintf(priority, 1, format, ap);

	va_end(ap);
}

int
create_socket(const char *addr, const char *port_s, int server)
{
	int gai_res;
	struct addrinfo ai_hints;
	struct addrinfo *ai_res, *ai_iter;
	int sock;

	sock = -1;

	memset(&ai_hints, 0, sizeof(ai_hints));
	ai_hints.ai_family = AF_INET;
	ai_hints.ai_socktype = SOCK_DGRAM;

	if (server) {
		ai_hints.ai_flags = AI_PASSIVE;
	}

	gai_res = getaddrinfo(addr, port_s, &ai_hints, &ai_res);
	if (gai_res != 0) {
		errx(1, "getaddrinfo failed: %s\n", gai_strerror(gai_res));
	}

	for (ai_iter = ai_res; ai_iter != NULL; ai_iter = ai_iter->ai_next) {
		if (ai_iter->ai_family != AF_INET) {
			continue ;
		}

		sock = socket(ai_iter->ai_family, ai_iter->ai_socktype, ai_iter->ai_protocol);
		if (sock == -1) {
			continue ;
		}

		if (!server) {
			if (connect(sock, ai_iter->ai_addr, ai_iter->ai_addrlen) != -1) {
				break ;
			}
		} else {
			if (bind(sock, ai_iter->ai_addr, ai_iter->ai_addrlen) != -1) {
				break ;
			}
		}

		close(sock);
	}

	if (ai_iter == NULL) {
		errx(1, "No available address");
	}

	freeaddrinfo(ai_res);

	return (sock);
}

void
server_loop(int sockfd)
{
	char msg[MAX_MSG_SIZE];
	int res;
	struct msghdr mhdr;
	struct sockaddr_storage from_addr;
	struct iovec msg_iovec;
	ssize_t recv_size;
	ssize_t send_size;

	while (1) {
		memset(&msg_iovec, 0, sizeof(msg_iovec));
		msg_iovec.iov_base = msg;
		msg_iovec.iov_len = MAX_MSG_SIZE;

		memset(&mhdr, 0, sizeof(mhdr));
		mhdr.msg_name = &from_addr;
		mhdr.msg_namelen = sizeof(struct sockaddr_storage);
		mhdr.msg_iov = &msg_iovec;
		mhdr.msg_iovlen = 1;

		recv_size = recvmsg(sockfd, &mhdr, 0);
		if (recv_size == -1) {
			log_err(LOG_ERR, "recvmsg failed");
			continue ;
		}

		if (mhdr.msg_flags & MSG_TRUNC) {
			log_printf(LOG_ERR, "packet truncated");
			continue ;
		}

		msg_iovec.iov_len = recv_size;
		send_size = sendmsg(sockfd, &mhdr, 0);
		if (send_size == -1) {
			log_err(LOG_ERR, "sendmsg failed");
			continue ;
		}
	}
}

int
main(int argc, char * argv[])
{
	int ch;
	int server_mode;
	char *port_s;
	int sockfd;

	server_mode = 0;
	port_s = "5405";

	while ((ch = getopt(argc, argv, "sp:")) != -1) {
		switch (ch) {
		case 's':
			server_mode = 1;
			break;
		case 'p':
			port_s = optarg;
			break;
		case '?':
			usage();
			break;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc != 1) {
		usage();
	}

	sockfd = create_socket(argv[0], port_s, server_mode);
	assert(sockfd != -1);

	if (server_mode) {
		server_loop(sockfd);
	}
	close(sockfd);

	return (EXIT_SUCCESS);
}
