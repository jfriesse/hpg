#include <stdio.h>

#include "nss-sock.h"

struct list_item {
	PRCList list; /* Have to be first item */

	int i;
};

static PRCList items;

void
dump_list(void)
{
	struct list_item *iter;

	printf("List content:\n");
	for (iter = (struct list_item *)PR_LIST_HEAD(&items); iter != (struct list_item *)&items; iter = (struct list_item *)PR_NEXT_LINK(&iter->list)) {
		printf("%u\n", iter->i);
	}
}

int
main(void)
{
	int i;
	struct list_item *item, *item2;
	PRIntervalTime epoch = PR_IntervalNow();
	PRIntervalTime now = PR_IntervalNow();
	int j = 0;

	PR_INIT_CLIST(&items);

	for (i = 0; i < 10; i++) {
		item = calloc(1, sizeof(*item));
		item->i = i;
		PR_APPEND_LINK(&item->list, &items);
	}

	dump_list();

	/*
	 * Delete 3rd item
	 */
	item = (struct list_item *)PR_LIST_HEAD(&items);
	item = (struct list_item *)PR_NEXT_LINK(&item->list);
	item = (struct list_item *)PR_NEXT_LINK(&item->list);
	item2 = (struct list_item *)PR_NEXT_LINK(&item->list);
	PR_REMOVE_AND_INIT_LINK(&item->list);
	dump_list();

	/*
	 * And insert it after next item
	 */
	PR_INSERT_AFTER(&item->list, &item2->list);
	dump_list();

	/*
	 * Empty list
	 */
	while (!PR_CLIST_IS_EMPTY(&items)) {
		item = (struct list_item *)PR_LIST_HEAD(&items);
		PR_REMOVE_AND_INIT_LINK(&item->list);
		free(item);
	}
	dump_list();

	/*
	 * Test timer
	 */
	while (1) {
		now = PR_IntervalNow();

		if ((PRIntervalTime)(now - epoch) > PR_MillisecondsToInterval(100)) {
			/*
			 * Timeout
			 */
			break ;
		} else {
			j++;
		}
	}

	printf("No cycles = %u, interval = %u\n", j, (PRIntervalTime)(now - epoch));

	return (0);
}
