#include <stdio.h>
#include "../src/map.h"

void mapdelete(char *key, void *val)
{
	printf("%s: %s\n", key, (char*)val);
}

int main()
{
	map_t *map = map_new(mapdelete);
	map_add(map, "sujan", "dutta");
	map_add(map, "mykey", "myvalue");

	char *val = map_lookup(map, "mykey");
	printf("%s\n", val);

	map_del(map);
	return 0;
}


