#include <assert.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/time.h>

#include "timer-list.h"

static PRIntervalTime global_interval;
static int tlist_cb_call_again;
static int tlist_cb_val;

PRIntervalTime PR_IntervalNow(void)
{

	return (global_interval);
}

int tlist_cb(void *data1, void *data2)
{
	unsigned int *i;

	i = (unsigned int *)data1;

	tlist_cb_val = *i;

	return (tlist_cb_call_again ? -1 : 0);
}

uint64_t
time_absdiff(struct timeval t1, struct timeval t2)
{
	uint64_t u64t1, u64t2, tmp;

	u64t1 = t1.tv_usec / 1000 + t1.tv_sec * 1000;
	u64t2 = t2.tv_usec / 1000 + t2.tv_sec * 1000;

	if (u64t2 > u64t1) {
		tmp = u64t1;
		u64t1 = u64t2;
		u64t2 = tmp;
	}

	return (u64t1 - u64t2);
}

#define SPEED_TEST_NO_ITEMS		 1000
#define SPEED_TEST_NO_CYCLES		   10

int
main(void)
{
	struct timer_list_entry t1;
	struct timer_list_entry t2;
	PRIntervalTime ct;
	uint64_t half_interval_u64;
	uint32_t half_interval_u32;
	uint16_t half_interval_u16;
	struct timer_list_entry *tle1;
	struct timer_list_entry *tle2;
	struct timer_list_entry *tle3;
	struct timer_list tlist;
	struct timer_list_entry *entry;
	unsigned int i1;
	unsigned int i2;
	unsigned int i3;
	unsigned int i;
	struct timer_list_entry *tlentries[SPEED_TEST_NO_ITEMS];
	struct timeval tstart, tend;

	i1 = 1;
	i2 = 2;
	i3 = 3;

	t1.expire_time = 10;
	t2.expire_time = 20;
	ct = 0;

/*	assert(timer_list_entry_cmp(&t1, &t2, ct) < 0);
	assert(timer_list_entry_cmp(&t2, &t1, ct) > 0);*/

	ct = 30;
/*	assert(timer_list_entry_cmp(&t2, &t1, ct) == 0);*/

	ct = ~0;
	ct /= 2;
/*	assert(timer_list_entry_cmp(&t2, &t1, ct) == 0);*/

	ct = ~0;
	ct = ct / 4 * 3;
/*	assert(timer_list_entry_cmp(&t1, &t2, ct) < 0);*/

	t1.expire_time = 0;
	t2.expire_time = ct;
/*	assert(timer_list_entry_cmp(&t1, &t2, ct) > 0);*/

	half_interval_u64 = ~0;
	half_interval_u64 /= 2;

	half_interval_u32 = ~0;
	half_interval_u32 /= 2;

	half_interval_u16 = ~0;
	half_interval_u16 /= 2;

	assert(half_interval_u64 == UINT64_MAX / 2);
	assert(half_interval_u32 == UINT32_MAX / 2);
	assert(half_interval_u16 == UINT16_MAX / 2);

	timer_list_init(&tlist);
	global_interval = 0;
	tle1 = timer_list_add(&tlist, 500, tlist_cb, NULL, NULL);
	tle2 = timer_list_add(&tlist, 1000, tlist_cb, NULL, NULL);
	tle3 = timer_list_add(&tlist, 1500, tlist_cb, NULL, NULL);

	entry = TAILQ_FIRST(&tlist.list);
	assert(entry != NULL && entry->interval == 500);
	assert(entry->expire_time == global_interval + PR_MillisecondsToInterval(500));
	entry = TAILQ_NEXT(entry, entries);
	assert(entry != NULL && entry->interval == 1000);
	entry = TAILQ_NEXT(entry, entries);
	assert(entry != NULL && entry->interval == 1500);
	entry = TAILQ_NEXT(entry, entries);
	assert(entry == NULL);
	assert(timer_list_time_to_expire(&tlist) == PR_MillisecondsToInterval(500));
	timer_list_delete(&tlist, tle1);
	timer_list_delete(&tlist, tle1);
	assert(timer_list_time_to_expire(&tlist) == PR_MillisecondsToInterval(1000));
	timer_list_free(&tlist);
	assert(timer_list_time_to_expire(&tlist) == PR_INTERVAL_NO_TIMEOUT);

	global_interval = ~0;
	tle2 = timer_list_add(&tlist, 1000, tlist_cb, NULL, NULL);
	tle1 = timer_list_add(&tlist, 500, tlist_cb, NULL, NULL);
	tle3 = timer_list_add(&tlist, 1500, tlist_cb, NULL, NULL);

	entry = TAILQ_FIRST(&tlist.list);
	assert(entry != NULL && entry->interval == 500);
	assert(entry->expire_time == global_interval + PR_MillisecondsToInterval(500));
	entry = TAILQ_NEXT(entry, entries);
	assert(entry != NULL && entry->interval == 1000);
	entry = TAILQ_NEXT(entry, entries);
	assert(entry != NULL && entry->interval == 1500);
	entry = TAILQ_NEXT(entry, entries);
	assert(entry == NULL);
	assert(timer_list_time_to_expire(&tlist) == PR_MillisecondsToInterval(500));
	timer_list_delete(&tlist, tle2);
	assert(timer_list_time_to_expire(&tlist) == PR_MillisecondsToInterval(500));
	timer_list_free(&tlist);

	global_interval = ~0;
	global_interval /= 2;
	global_interval += 10000;
	tle3 = timer_list_add(&tlist, 1500, tlist_cb, NULL, NULL);
	tle2 = timer_list_add(&tlist, 1000, tlist_cb, NULL, NULL);
	tle1 = timer_list_add(&tlist, 500, tlist_cb, NULL, NULL);

	entry = TAILQ_FIRST(&tlist.list);
	assert(entry != NULL && entry->interval == 500);
	assert(entry->expire_time == global_interval + PR_MillisecondsToInterval(500));
	entry = TAILQ_NEXT(entry, entries);
	assert(entry != NULL && entry->interval == 1000);
	entry = TAILQ_NEXT(entry, entries);
	assert(entry != NULL && entry->interval == 1500);
	entry = TAILQ_NEXT(entry, entries);
	assert(entry == NULL);
	assert(timer_list_time_to_expire(&tlist) == PR_MillisecondsToInterval(500));
	timer_list_delete(&tlist, tle3);
	assert(timer_list_time_to_expire(&tlist) == PR_MillisecondsToInterval(500));
	timer_list_free(&tlist);

	global_interval = 0;
	tle1 = timer_list_add(&tlist, 1500, tlist_cb, NULL, NULL);
	global_interval = 500;
	tle2 = timer_list_add(&tlist, 500, tlist_cb, NULL, NULL);
	global_interval = 10000;
	tle3 = timer_list_add(&tlist, 1000, tlist_cb, NULL, NULL);

	entry = TAILQ_FIRST(&tlist.list);
	assert(entry != NULL && entry->interval == 500);
	entry = TAILQ_NEXT(entry, entries);
	assert(entry != NULL && entry->interval == 1500);
	entry = TAILQ_NEXT(entry, entries);
	assert(entry != NULL && entry->interval == 1000);
	entry = TAILQ_NEXT(entry, entries);
	assert(entry == NULL);
	timer_list_free(&tlist);

	global_interval = 0;
	tlist_cb_val = 0;
	tlist_cb_call_again = 0;
	tle1 = timer_list_add(&tlist, 500, tlist_cb, &i1, NULL);
	tle2 = timer_list_add(&tlist, 1000, tlist_cb, &i2, NULL);
	tle3 = timer_list_add(&tlist, 1500, tlist_cb, &i3, NULL);

	timer_list_expire(&tlist);
	assert(tlist_cb_val == 0);
	global_interval = 500;
	tlist_cb_val = 0;
	timer_list_expire(&tlist);
	assert(tlist_cb_val == i1);
	tlist_cb_val = 0;
	timer_list_expire(&tlist);
	assert(tlist_cb_val == 0);

	global_interval = 1000;
	tlist_cb_val = 0;
	tlist_cb_call_again = 1;
	timer_list_expire(&tlist);
	assert(tlist_cb_val == i2);

	global_interval = 1500;
	tlist_cb_val = 0;
	timer_list_expire(&tlist);
	assert(tlist_cb_val == i3);

	global_interval = 2000;
	tlist_cb_val = 0;
	timer_list_expire(&tlist);
	assert(tlist_cb_val == i2);

	global_interval = 2500;
	tlist_cb_val = 0;
	timer_list_expire(&tlist);
	assert(tlist_cb_val == 0);

	timer_list_delete(&tlist, tle3);
	assert(timer_list_time_to_expire(&tlist) == PR_MillisecondsToInterval(500));

	global_interval = 3000;
	tlist_cb_val = 0;
	timer_list_expire(&tlist);
	assert(tlist_cb_val == i2);

	global_interval = 0;
	tlist_cb_val = 0;
	tlist_cb_call_again = 0;
	tle1 = timer_list_add(&tlist, 500, tlist_cb, &i1, NULL);
	tle2 = timer_list_add(&tlist, 1000, tlist_cb, &i2, NULL);
	tle3 = timer_list_add(&tlist, 1500, tlist_cb, &i3, NULL);
	global_interval = 100;
	assert(timer_list_time_to_expire(&tlist) == PR_MillisecondsToInterval(400));
	timer_list_reschedule(&tlist, tle1);
	assert(timer_list_time_to_expire(&tlist) == PR_MillisecondsToInterval(500));
	global_interval = 2000;
	timer_list_reschedule(&tlist, tle2);
	global_interval = 0;
	assert(timer_list_time_to_expire(&tlist) == PR_MillisecondsToInterval(600));
	timer_list_delete(&tlist, tle1);
	assert(timer_list_time_to_expire(&tlist) == PR_MillisecondsToInterval(1500));
	timer_list_delete(&tlist, tle3);
	assert(timer_list_time_to_expire(&tlist) == PR_MillisecondsToInterval(3000));
	timer_list_free(&tlist);

	gettimeofday(&tstart, NULL);
	global_interval = 0;
	for (i = 0; i < SPEED_TEST_NO_ITEMS; i++) {
		tlentries[i] = timer_list_add(&tlist, (i + 1) * 10, tlist_cb, &i1, NULL);
		assert(tlentries[i] != NULL);
	}

	for (i = 0; i < SPEED_TEST_NO_CYCLES; i++) {
		global_interval = i * 10;

		for (i1 = 0; i1 < SPEED_TEST_NO_ITEMS; i1++) {
			timer_list_reschedule(&tlist, tlentries[i1]);
		}

		assert(timer_list_time_to_expire(&tlist) == PR_MillisecondsToInterval(10));
	}
	gettimeofday(&tend, NULL);

	printf("Time to finish: %"PRIu64, time_absdiff(tstart, tend));

	return (0);
}
