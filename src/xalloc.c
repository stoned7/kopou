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
	ptr = malloc(size);
	if (ptr == NULL)
		xalloc_oom_handler(size);
	return ptr;
}


void *xcalloc(size_t nmemb, size_t size)
{
	void *ptr;
	ptr = calloc(nmemb, size);
	if (ptr == NULL)
		xalloc_oom_handler(nmemb * size);
	return ptr;
}

void *xrealloc(void *ptr, size_t size)
{
	void *new_ptr;
	new_ptr = realloc(ptr, size);
	if (new_ptr == NULL)
		xalloc_oom_handler(size);
	return new_ptr;
}

void xfree(void *ptr)
{
	free(ptr);
	ptr = NULL;
}
