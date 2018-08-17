#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <poll.h>
#include <err.h>
#include <time.h>

#include <corosync/corotypes.h>
#include <corosync/cpg.h>
#include <corosync/quorum.h>

static void
print_time(void)
{
	struct timeval tv;
	struct tm *tmp;
	char time_str[128];

	if (gettimeofday(&tv, NULL) != 0) {
		err(3, "gettimeofday");
	}

	if ((tmp = localtime(&tv.tv_sec)) == NULL) {
		err(3, "local_time");
	}

	if (strftime(time_str, sizeof(time_str), "%H:%M:%S", tmp) == 0) {
		err(3, "strftime");
	}

	printf("%s.%03ld", time_str, (tv.tv_usec / 1000));
}

static void
cpg_totem_confchg_callback(cpg_handle_t handle,
    struct cpg_ring_id ring_id,
    uint32_t member_list_entries,
    const uint32_t *member_list)
{
	int i;

	print_time();
	printf(" cpg_totem_confchg_callback ringid=%x.%"PRIx64" members=", ring_id.nodeid, ring_id.seq);

	for (i = 0; i < member_list_entries; i++) {
		if (i != 0) {
			printf(",");
		}

		printf("%x", member_list[i]);
	}
	printf ("\n");
}

static void
print_cpgname(const struct cpg_name *name)
{
        int i;

        for (i = 0; i < name->length; i++) {
                printf("%c", name->value[i]);
        }
}

static void cpg_confchg_callback(cpg_handle_t handle,
	const struct cpg_name *group_name,
	const struct cpg_address *member_list, size_t member_list_entries,
	const struct cpg_address *left_list, size_t left_list_entries,
	const struct cpg_address *joined_list, size_t joined_list_entries)
{
	int i;

	print_time();
	printf(" cpg_confchg_callback group=");
	print_cpgname(group_name);
	printf(" joined=");
	for (i = 0; i < joined_list_entries; i++) {
		if (i != 0) {
			printf(",");
		}
		printf("(%x %x)", joined_list[i].nodeid, joined_list[i].pid);
	}
	printf(" left=");

	for (i = 0; i < left_list_entries; i++) {
		if (i != 0) {
			printf(",");
		}
		printf("(%x %x)", left_list[i].nodeid, left_list[i].pid);
	}
	printf(" members=");

	for (i = 0; i < member_list_entries; i++) {
		if (i != 0) {
			printf(",");
		}
		printf("(%x %x)", member_list[i].nodeid, member_list[i].pid);
	}
	printf("\n");
}

void
quorum_notify_callback(quorum_handle_t handle, uint32_t quorate, uint64_t ring_seq,
    uint32_t view_list_entries, uint32_t *view_list)
{
	int i;

	print_time();
	printf(" quorum_notify_callback quorate=%u ", quorate);
	printf("ringseq=%"PRIx64" viewlist=", ring_seq);

	for (i = 0; i < view_list_entries; i++) {
		if (i != 0) {
			printf(",");
		}
		printf("%x", view_list[i]);
	}

	printf("\n");
}

static cpg_model_v1_data_t cpg_model_data = {
	.cpg_deliver_fn = NULL,
	.cpg_confchg_fn = cpg_confchg_callback,
	.cpg_totem_confchg_fn = cpg_totem_confchg_callback,
	.flags = CPG_MODEL_V1_DELIVER_INITIAL_TOTEM_CONF,
};

static quorum_callbacks_t quorum_callbacks = {
	.quorum_notify_fn = quorum_notify_callback,
};

int
main(void)
{
	cpg_handle_t cpg_handle;
	quorum_handle_t quorum_handle;
	struct cpg_name group_name;
	cs_error_t cs_err;
	struct pollfd pfds[2];
	int cpg_fd;
	int quorum_fd;
	int poll_res;
	int quorum_type;

	strcpy(group_name.value, "CPGSERVORDER");
	group_name.length = strlen(group_name.value);

	if ((cs_err = cpg_model_initialize(&cpg_handle, CPG_MODEL_V1,
	    (cpg_model_data_t *)&cpg_model_data, NULL)) != CS_OK) {
		printf("Could not initialize Cluster Process Group API instance error %d\n", cs_err);
		exit(1);
	}

	assert(cpg_join(cpg_handle, &group_name) == CS_OK);
	cpg_fd_get(cpg_handle, &cpg_fd);

	if ((cs_err = quorum_initialize(&quorum_handle, &quorum_callbacks, &quorum_type)) != CS_OK) {
		printf("Could not initialize Quorum API instance error %d\n", cs_err);
		exit(1);
	}

	assert(quorum_trackstart(quorum_handle, CS_TRACK_CURRENT | CS_TRACK_CHANGES) == CS_OK);
	quorum_fd_get(quorum_handle, &quorum_fd);

	setlinebuf(stdout);

	while (1) {
		pfds[0].fd = cpg_fd;
		pfds[0].events = POLLIN;
		pfds[0].revents = 0;

		pfds[1].fd = quorum_fd;
		pfds[1].events = POLLIN;
		pfds[1].revents = 0;

		poll_res = poll(pfds, 2, -1);
		if (poll_res < 0) {
			err(1, "poll_res < 0");
		}

		if ((pfds[0].revents & POLLIN) && (pfds[1].revents & POLLIN)) {
			print_time();
			printf(" cpg&quorum pollin\n");
		} else if (pfds[0].revents & POLLIN) {
			print_time();
			printf(" cpg pollin\n");
		} else if (pfds[1].revents && POLLIN) {
			print_time();
			printf(" quorum pollin\n");
		}

		if (pfds[0].revents & POLLIN) {
			if (cpg_dispatch(cpg_handle, CS_DISPATCH_ALL) != CS_OK) {
				printf("cpg_dispatch error %d\n", cs_err);
				exit(1);
			}
		}

		if (pfds[1].revents & POLLIN) {
			if (quorum_dispatch(quorum_handle, CS_DISPATCH_ALL) != CS_OK) {
				printf("quorum_dispatch error %d\n", cs_err);
				exit(1);
			}
		}
	}

	return (0);
}
