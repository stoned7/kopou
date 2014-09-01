#include <stdio.h>
#include "../src/kstring.h"
#include "../src/kopou.h"

int main()
{
	kstr_t s = kstr_len_new("sujan dutta", 11);
	int len = kstr_len(s);
	printf("sl: %d\n", len);
	printf("s: %s\n", s);
	kstr_del(s);
	len = kstr_len(s);
	printf("sl: %d\n", len);
	printf("s: %s\n", s);

	kstr_t s1 = kstr_new("hello there, how are you doing");
	int len1 = kstr_len(s1);
	printf("s1l: %d\n", len1);
	printf("s1: %s\n", s1);
	kstr_del(s1);
	len1 = kstr_len(s1);
	printf("s1l: %d\n", len1);
	printf("s1: %s\n", s1);
	printf("%d\n", KKK);
	return EXIT_SUCCESS;
}
