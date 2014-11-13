#ifndef __XALLOC_H__
#define __XALLOC_H__

/*
 * required to set oom_handler
 * dont call allocation with size 0, oom_handler perhaps invoked.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

size_t xalloc_total_mem_used(void);

void xalloc_set_oom_handler(void (*oom_handler)(size_t));

void *xmalloc(size_t size);
void *xcalloc(size_t nmemb, size_t size);
void *xrealloc(void *p, size_t size);
void xfree(void *p);

#endif
