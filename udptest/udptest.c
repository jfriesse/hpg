#include <sys/types.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <inttypes.h>
#include <netdb.h>
#include <poll.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#define MAX_MSG_SIZE	512
#define PROTOCOL_ID	"UDPTest"

static const char log_month_str[][4] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static int show_debug = 0;

void
usage(void)
{

	printf("usage: udptest [-sv] [-i interval] ip_addr\n");
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

	if (priority == LOG_DEBUG && !show_debug) {
		return ;
	}

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

void
client_send_msg(int sockfd, uint32_t seq, size_t pad_len)
{
	char msg[MAX_MSG_SIZE];
	struct msghdr mhdr;
	struct iovec msg_iovec;
	uint32_t u32n;
	size_t msg_len;
	ssize_t send_size;

	memset(msg, 0, MAX_MSG_SIZE);

	msg_len = 0;
	/*
	 * Add protocol id
	 */
	memcpy(msg + msg_len, PROTOCOL_ID, strlen(PROTOCOL_ID));
	msg_len += strlen(PROTOCOL_ID);

	/*
	 * Add seq
	 */
	u32n = htonl(seq);
	memcpy(msg + msg_len, &u32n, sizeof(u32n));
	msg_len += sizeof(u32n);

	/*
	 * Add pad
	 */
	msg_len += pad_len;

	memset(&msg_iovec, 0, sizeof(msg_iovec));
	msg_iovec.iov_base = msg;
	msg_iovec.iov_len = msg_len;

	memset(&mhdr, 0, sizeof(mhdr));
	mhdr.msg_iov = &msg_iovec;
	mhdr.msg_iovlen = 1;

	send_size = sendmsg(sockfd, &mhdr, 0);
	if (send_size == -1) {
		log_err(LOG_ERR, "sendmsg failed");
	}
}

uint64_t
get_tstamp(void)
{
	struct timeval tv;
	int res;
	uint64_t rv;

	res = gettimeofday(&tv, NULL);
	assert(res == 0);

	rv = (tv.tv_sec * 1000) + tv.tv_usec / 1000;
}

int
client_recv_msg(int sockfd, uint32_t *seq)
{
	char msg[MAX_MSG_SIZE];
	struct msghdr mhdr;
	struct sockaddr_storage from_addr;
	int res;
	struct iovec msg_iovec;
	ssize_t recv_size;
	uint32_t u32n;

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
		return (-1);
	}

	if (mhdr.msg_flags & MSG_TRUNC) {
		log_printf(LOG_WARNING, "packet truncated");
	}

	if ((msg_iovec.iov_len < strlen(PROTOCOL_ID) + sizeof(u32n)) ||
	    (memcmp(msg, PROTOCOL_ID, strlen(PROTOCOL_ID)) != 0)) {
		log_printf(LOG_ERR, "received packet is not ours");
		return (-1);
	}

	memcpy(&u32n, msg + strlen(PROTOCOL_ID), sizeof(u32n));
	*seq = ntohl(u32n);

	return (0);
}

int
client_wait_for_reply(int sockfd, int *interval, uint64_t expected_seq, int *expected_seq_received)
{
	struct pollfd pfd;
	uint64_t pre_poll_tstamp;
	uint64_t post_poll_tstamp;
	int timeout;
	int poll_res;
	int res;
	uint32_t seq;
	int ret;

	pre_poll_tstamp = get_tstamp();

	memset(&pfd, 0, sizeof(pfd));
	pfd.fd = sockfd;
	pfd.events = POLLIN;

	poll_res = poll(&pfd, 1, *interval);
	ret = poll_res;
	post_poll_tstamp = get_tstamp();

	*interval -= (post_poll_tstamp - pre_poll_tstamp);
	if (*interval < 0) {
		*interval = 0;
		ret = 0;
	}

	switch (poll_res) {
	case -1:
		log_err(LOG_ERR, "Poll failed");
		break;
	case 0:
		break;
	case 1:
		if (pfd.revents & POLLERR || pfd.revents & POLLHUP || pfd.revents & POLLNVAL) {
			log_printf(LOG_WARNING, "Poll error. Revents = %d", pfd.revents);
			ret = -1;
		} else if (pfd.revents & POLLIN) {
			/*
			 * Receive message
			 */
			res = client_recv_msg(sockfd, &seq);
			if (res == 0) {
				log_printf(LOG_DEBUG, "Received message with seq no %" PRIu32, seq);
				if (seq == expected_seq) {
					(*expected_seq_received)++;
				} else {
					log_printf(LOG_ERR, "Received message with unexpected seq no %" PRIu32, seq);
				}
			}
		}
		break;
	}

	return (ret);
}

void
client_loop(int sockfd, int interval)
{
	uint32_t seq;
	int timeout;
	int cont;
	int res;
	int received;

	seq = 0;

	while (1) {
		seq++;

		client_send_msg(sockfd, seq, 0);

		timeout = interval;
		received = 0;

		while ((res = client_wait_for_reply(sockfd, &timeout, seq, &received) == 1)) ;

		if (res == -1) {
			poll(NULL, 0, timeout);
		} else if (res == 0) {
			if (received == 0) {
				log_printf(LOG_ERR, "Reply with seq %" PRIu32 " lost", seq);
			} else if (received > 1) {
				log_printf(LOG_ERR, "Reply with seq %" PRIu32 " received %u times", seq, received);
			}
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
	int interval;
	double numd;
	char *ep;

	server_mode = 0;
	show_debug = 0;
	port_s = "5405";
	interval = 1000;

	while ((ch = getopt(argc, argv, "svi:p:")) != -1) {
		switch (ch) {
		case 's':
			server_mode = 1;
			break;
		case 'v':
			show_debug = 1;
			break;
		case 'i':
			numd = strtod(optarg, &ep);
			if (numd < 0 || *ep != '\0' || numd * 1000 > INT32_MAX) {
				errx(1, "illegal number, -i argument -- %s", optarg);
			}

			interval = (int)(numd * 1000.0);
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
	} else {
		client_loop(sockfd, interval);
	}

	close(sockfd);

	return (EXIT_SUCCESS);
}
