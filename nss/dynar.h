#ifndef _DYNAR_H_
#define _DYNAR_H_

#include <sys/types.h>
#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Dynamic array structure
 */
struct dynar {
	char *data;
	size_t size;
	size_t allocated;
	size_t maximum_size;
};

extern void	 dynar_init(struct dynar *array, size_t maximum_size);

extern void	 dynar_destroy(struct dynar *array);

extern void	 dynar_clean(struct dynar *array);

extern size_t	 dynar_size(struct dynar *array);

extern size_t	 dynar_max_size(struct dynar *array);

extern char	*dynar_data(struct dynar *array);

extern int	 dynar_cat(struct dynar *array, char *src, size_t size);


#ifdef __cplusplus
}
#endif

#endif /* _DYNAR_H_ */
