#include "xalloc.h"
#include <stdint.h>

#define SSIZE (sizeof(size_t) * 2)

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
	size_t rsize;

	rsize = SSIZE + size;
	ptr = malloc(rsize);
	if (ptr == NULL)
		xalloc_oom_handler(rsize);

	*(size_t*)ptr = rsize;
	total_mem_used += rsize;

	return (char*)ptr + SSIZE;
}

void *xcalloc(size_t nmemb, size_t size)
{
	void *ptr;
	size_t rsize;

	rsize = SSIZE + (nmemb * size);
	ptr = calloc(1, rsize);
	if (ptr == NULL)
		xalloc_oom_handler(rsize);

	*(size_t*)ptr = rsize;
	total_mem_used += rsize;
	return (char*)ptr + SSIZE;
}

void *xrealloc(void *ptr, size_t size)
{
	size_t old_size, rsize;
	void *new_ptr, *old_ptr;

	if (ptr == NULL)
		return xmalloc(size);

	old_ptr = (char*)ptr - SSIZE;
	old_size = *(size_t*)old_ptr;

	rsize = SSIZE + size;
	new_ptr = realloc(old_ptr, rsize);
	if (new_ptr == NULL)
		xalloc_oom_handler(rsize);

	total_mem_used -= old_size;

	*(size_t*)new_ptr = rsize;
	total_mem_used += rsize;

	return (char*)new_ptr + SSIZE;
}

void xfree(void *ptr)
{
	size_t size;
	void *rptr;

	if (ptr == NULL)
		return;

	rptr = (char*)ptr - SSIZE;
	size = *(size_t*)rptr;
	free(rptr);

	total_mem_used -= size;
}
