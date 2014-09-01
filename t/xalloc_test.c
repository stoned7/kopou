#include <stdio.h>
#include <stdlib.h>

#include "../src/xalloc.h"

void test_oom_handler(size_t size)
{
	fprintf(stderr, "out of memory: %lu\n", size);
	perror("test_oom_handler()");
	exit(EXIT_FAILURE);
}

int main()
{
	void *obj, *obj1;
	size_t obj_size;
	xalloc_set_oom_handler(test_oom_handler);

	obj_size = 1024 * 1024;
	obj = xmalloc(obj_size);
	fprintf(stdout,"Total memory used: %lu\n", xalloc_total_mem_used());
	xfree(obj);
	fprintf(stdout,"Total memory used(after free): %lu\n",xalloc_total_mem_used());

	obj_size = 512 * (1024 * 1024);
	obj = xcalloc(14, obj_size);
	fprintf(stdout,"Total memory used: %lu\n", xalloc_total_mem_used());

	obj1 = xmalloc(obj_size);
	fprintf(stdout,"Total memory used: %lu\n", xalloc_total_mem_used());

	xfree(obj);
	fprintf(stdout,"Total memory used(after free): %lu\n", xalloc_total_mem_used());
	xfree(obj1);
	fprintf(stdout,"Total memory used(after free): %lu\n",xalloc_total_mem_used());

	return EXIT_SUCCESS;
}
