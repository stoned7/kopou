#include <stdio.h>
#include <stdlib.h>

#include "../src/common.h"
#include "../src/xalloc.h"
#include "../src/kstring.h"

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

	kstr_t name = kstr_new("sujan");
	kstr_t name2 = kstr_new("dutta");

	obj_size = 1024;
	obj = xmalloc(obj_size);
	fprintf(stdout,"Total memory used: %lu\n", xalloc_total_mem_used());

	kstr_del(name);
	kstr_del(name2);

	obj = xrealloc(obj, 1024 + 100);

	void *test = xcalloc(3, 10);
	obj1 = xmalloc(obj_size);
	fprintf(stdout,"Total memory used: %lu\n", xalloc_total_mem_used());
	xfree(obj);
	xfree(obj1);
	xfree(test);
	fprintf(stdout,"Total memory used(after free): %lu\n",xalloc_total_mem_used());
/*
	obj_size = 512 * (1024 * 1024);
	obj = xcalloc(14, obj_size);
	fprintf(stdout,"Total memory used: %lu\n", xalloc_total_mem_used());

	obj1 = xmalloc(obj_size);
	fprintf(stdout,"Total memory used: %lu\n", xalloc_total_mem_used());

	xfree(obj);
	fprintf(stdout,"Total memory used(after free): %lu\n", xalloc_total_mem_used());
	xfree(obj1);
	fprintf(stdout,"Total memory used(after free): %lu\n",xalloc_total_mem_used());

*/
	return EXIT_SUCCESS;
}
