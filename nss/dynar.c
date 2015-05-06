#include <sys/types.h>

#include <stdlib.h>
#include <string.h>

#include "dynar.h"

void
dynar_init(struct dynar *array, size_t maximum_size)
{

	memset(array, 0, sizeof(*array));
	array->maximum_size = maximum_size;
}

void
dynar_destroy(struct dynar *array)
{

	free(array->data);
	dynar_init(array, array->maximum_size);
}

void
dynar_clean(struct dynar *array)
{

	array->size = 0;
}

size_t
dynar_size(struct dynar *array)
{

	return (array->size);
}

size_t
dynar_max_size(struct dynar *array)
{

	return (array->maximum_size);
}

char *
dynar_data(struct dynar *array)
{

	return (array->data);
}

static int
dynar_realloc(struct dynar *array, size_t new_array_size)
{
	char *new_data;

	if (new_array_size > array->maximum_size) {
		return (-1);
	}

	new_data = realloc(array->data, new_array_size);

	if (new_data == NULL) {
		return (-1);
	}

	array->allocated = new_array_size;
	array->data = new_data;

	return (0);
}

int
dynar_cat(struct dynar *array, const void *src, size_t size)
{
	size_t new_size;

	if (array->size + size > array->maximum_size) {
		return (-1);
	}

	if (array->size + size > array->allocated) {
		new_size = (array->allocated + size) * 2;
		if (new_size > array->maximum_size) {
			new_size = array->maximum_size;
		}

		if (dynar_realloc(array, new_size) == -1) {
			return (-1);
		}
	}

	memmove(array->data + array->size, src, size);
	array->size += size;

	return (0);
}
