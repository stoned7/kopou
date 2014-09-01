#include <stdio.h>
#include "../src/kstring.h"

int main()
{
	kstr_t s = kstr_len_new("sujan dutta", 11);
	int len = kstr_len(s);
	kstr_del(s);

	kstr_t s1 = kstr_new("hello there, how are you doing");
	int len1 = kstr_len(s1);
	kstr_del(s1);

	return EXIT_SUCCESS;
}
