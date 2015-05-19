#include <string.h>

#include "timer-list.h"

void
timer_list_init(struct timer_list *tlist)
{

	memset(tlist, 0, sizeof(*tlist));

	TAILQ_INIT(&tlist->list);
	TAILQ_INIT(&tlist->free_list);
}

int
timer_list_add(struct timer_list *tlist, PRUint32 interval, timer_list_cb_fn func, void *data1, void *data2)
{
	struct timer_list_entry *entry;

	if (interval > (0xffffffffUL / 4)) {
		return (-1);
	}

	if (!TAILQ_EMPTY(&tlist->free_list)) {
		/*
		 * Use free list entry
		 */
		entry = TAILQ_FIRST(&tlist->free_list);
		TAILQ_REMOVE(&tlist->free_list, entry, entries);
	} else {
		/*
		 * Alloc new entry
		 */
		entry = malloc(sizeof(*entry));
		if (entry == NULL) {
			return (-1);
		}
	}

	memset(entry, 0, sizeof(*entry));
	entry->epoch = PR_IntervalNow();
	entry->interval = interval;
	entry->func = func;
	entry->user_data1 = data1;
	entry->user_data2 = data2;
	TAILQ_INSERT_TAIL(&tlist->list, entry, entries);

	return (0);
}

void
timer_list_expire(struct timer_list *tlist)
{
	PRIntervalTime now;
	struct timer_list_entry *entry;
	struct timer_list_entry *entry_next;
	PRUint32 delta;
	int res;

	now = PR_IntervalNow();

	entry = TAILQ_FIRST(&tlist->list);

	while (entry != NULL) {
		entry_next = TAILQ_NEXT(entry, entries);

		delta = PR_IntervalToMilliseconds(now - entry->epoch);
		if (delta >= entry->interval) {
			/*
			 * Expired
			 */
			res = entry->func(entry->user_data1, entry->user_data2);
			if (res == 0) {
				/*
				 * Move item to free list
				 */
				TAILQ_REMOVE(&tlist->list, entry, entries);
				TAILQ_INSERT_HEAD(&tlist->free_list, entry, entries);
			} else {
				/*
				 * Schedule again
				 */
				entry->epoch = now;
			}
		}

		entry = entry_next;
	}
}

PRIntervalTime
timer_list_time_to_expire(struct timer_list *tlist)
{
	PRIntervalTime now;
	struct timer_list_entry *entry;
	PRUint32 delta;
	PRUint32 timeout;
	PRUint32 min_timeout;
	int min_timeout_set;

	min_timeout_set = 0;

	now = PR_IntervalNow();

	TAILQ_FOREACH(entry, &tlist->list, entries) {
		delta = PR_IntervalToMilliseconds(now - entry->epoch);

		if (delta >= entry->interval) {
			/*
			 * One of timer already expired
			 */
			return (PR_INTERVAL_NO_WAIT);
		}

		timeout = entry->interval - delta;

		if (!min_timeout_set) {
			min_timeout_set = 1;
			min_timeout = timeout;
		}

		if (timeout < min_timeout) {
			min_timeout = timeout;
		}
	}

	if (!min_timeout_set) {
		return (PR_INTERVAL_NO_TIMEOUT);
	}

	return (PR_MillisecondsToInterval(min_timeout));
}

void
timer_list_free(struct timer_list *tlist)
{
	struct timer_list_entry *entry;
	struct timer_list_entry *entry_next;


	entry = TAILQ_FIRST(&tlist->list);

	while (entry != NULL) {
		entry_next = TAILQ_NEXT(entry, entries);

		free(entry);

		entry = entry_next;
	}

	entry = TAILQ_FIRST(&tlist->free_list);

	while (entry != NULL) {
		entry_next = TAILQ_NEXT(entry, entries);

		free(entry);

		entry = entry_next;
	}

	timer_list_init(tlist);
}
