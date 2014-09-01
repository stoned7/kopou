#include <stdio.h>
#include <string.h>

#include "../lib/crc16.h"


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

	return 0;
}
