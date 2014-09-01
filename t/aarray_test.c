#include <stdio.h>
#include "../src/aarray.h"

uint32_t hashfunction(const kstr_t key)
{
	return 0;
}
int key_comparer(const kstr_t key1, const kstr_t key2)
{
	return 0;
}
void remove_handler(kstr_t key, void *val)
{

}

int main()
{
	aarray_t *aa = aarray_new(4,hashfunction, key_comparer, remove_handler); 
	aarray_del(aa);
	return 0;
}
