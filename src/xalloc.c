#include "xalloc.h"

static void (*xalloc_oom_handler)(size_t);

void xalloc_set_oom_handler(void (*oom_handler)(size_t))
{
	xalloc_oom_handler = oom_handler;
}

void *xmalloc(size_t size)
{
	void *p;

	p = malloc(size);
	if (!p)
		xalloc_oom_handler(size);

	return p;
}


void *xcalloc(size_t nmemb, size_t size)
{
	void *p;

	p = calloc(nmemb, size);
	if (!p)
		xalloc_oom_handler(size);

	return p;
}

void *xrealloc(void *p, size_t size)
{
	void *np;

	if (!p)
		return xmalloc(size);

	np = realloc(p, size);
	if (!np)
		xalloc_oom_handler(size);

	return np;
}

void xfree(void *p)
{
	free(p);
	p = NULL;
}
