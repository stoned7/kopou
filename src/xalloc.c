#include "xalloc.h"

static size_t total_mem_used;
static void (*xalloc_oom_handler)(size_t);

size_t xalloc_total_mem_used(void)
{
	return total_mem_used;
}

void xalloc_set_oom_handler(void (*oom_handler)(size_t))
{
	xalloc_oom_handler = oom_handler;
}

void *xmalloc(size_t size)
{
	void *ptr;
	size_t actual_size;

	if (size <= 0)
		return NULL;

	ptr = malloc(size);
	if (ptr == NULL)
		xalloc_oom_handler(size);
	actual_size = malloc_usable_size(ptr);
	total_mem_used += actual_size;
	return ptr;
}


void *xcalloc(size_t nmemb, size_t size)
{
	void *ptr;
	size_t actual_size;

	if (size <= 0)
		return NULL;

	ptr = calloc(nmemb, size);
	if (ptr == NULL)
		xalloc_oom_handler(nmemb * size);

	actual_size = malloc_usable_size(ptr);
	total_mem_used += actual_size;
	return ptr;
}

void *xrealloc(void *ptr, size_t size)
{
	size_t old_size, new_size;
	void *new_ptr;

	if (size <= 0)
		return NULL;

	old_size = malloc_usable_size(ptr);
	new_ptr = realloc(ptr, size);

	if (new_ptr == NULL)
		xalloc_oom_handler(size);

	total_mem_used -= old_size;
	new_size = malloc_usable_size(new_ptr);
	total_mem_used += new_size;
	return new_ptr;
}

void xfree(void *ptr)
{
	size_t size;

	if (ptr == NULL)
		return;

	size = malloc_usable_size(ptr);
	free(ptr);
	total_mem_used -= size;
	ptr = NULL;
}
