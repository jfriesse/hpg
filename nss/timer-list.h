#ifndef _TIMER_LIST_H_
#define _TIMER_LIST_H_

#include <sys/queue.h>

#include <nspr.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*timer_list_cb_fn)(void *data1, void *data2);

struct timer_list_entry {
	PRIntervalTime epoch;
	PRUint32 interval;
	timer_list_cb_fn func;
	void *user_data1;
	void *user_data2;
	TAILQ_ENTRY(timer_list_entry) entries;
};

struct timer_list {
	TAILQ_HEAD(, timer_list_entry) list;
	TAILQ_HEAD(, timer_list_entry) free_list;
};

extern void		timer_list_init(struct timer_list *tlist);

extern int		timer_list_add(struct timer_list *tlist, PRUint32 interval,
	timer_list_cb_fn func, void *data1, void *data2);

extern void		timer_list_expire(struct timer_list *tlist);

extern PRIntervalTime	timer_list_time_to_expire(struct timer_list *tlist);

extern void		timer_list_free(struct timer_list *tlist);

#ifdef __cplusplus
}
#endif

#endif /* _TIMER_LIST_H_ */
