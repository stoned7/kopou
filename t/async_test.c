#include <stdio.h>
#include "../src/async.h"
#include <pthread.h>

void bgsave(void *arg)
{
	int i;
	i = *((int*)arg);
	printf("%zd: %d\n", pthread_self(), i);
}

int main()
{
	int i;
	async_t *a;

	a = async_new(1);

	for (i = 0; i < 10; i++) {
		async_add_task(a, bgsave, (int*)i, NULL);
	}

	//async_del(a);

	printf("done\n");
	while(1);
	return 0;
}
