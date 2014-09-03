#include <stdio.h>
#include <string.h>

#include "../lib/crc16.h"
#include "../src/kopou.h"


int main()
{
	unsigned long nobjs = 100000000;
	unsigned long i;

	int cluster[128] = {0};

	char *str = "__user_package__%d";
	char strkey[128];

	for (i = 0; i < nobjs; i++) {
		sprintf(strkey, str, i);
		uint16_t r = crc16(strkey, strlen(strkey));
		int clusterid = r & 127;
		cluster[clusterid]++;
	}

	int j;
	for (j = 0; j < 128; j++)
		printf("cluster (%d): %d\n", j, cluster[j]);

	vnodes_state_t bitarray;
	int i;
	printf("%d\n", _vnode_state_size_slots);


	vnode_state_add(bitarray, 55);
	vnode_state_add(bitarray, 127);
	vnode_state_add(bitarray, 77);
	vnode_state_remove(bitarray, 55);
	vnode_state_add(bitarray, 78);
	vnode_state_add(bitarray, 89);

	vnode_state_empty(bitarray);
	for (i = 0; i < VNODE_SIZE; i++){
		if (vnode_state_contain(bitarray, i))
			printf("%d: test yes\n", i);
	}

	vnode_state_fill(bitarray);
	for (i = 0; i < VNODE_SIZE; i++){
		if (vnode_state_contain(bitarray, i))
			printf("%d: test yes\n", i);
	}

	return 0;
}
