#include "kstring.h"
#include "xalloc.h"

#define KSTR_EOS '\0'

kstr_t kstr_len_new(char *str, size_t len)
{
	kstr_t kstr = malloc(lsize + len + 1);
	memcpy(kstr, &len, lsize);
	memcpy(kstr + lsize, str, len);
	kstr[lsize + len] = KSTR_EOS;
	return kstr + lsize;
}

kstr_t kstr_new(char *str)
{
	return kstr_len_new(str, strlen(str));
}

void kstr_del(kstr_t str)
{
	free(str - lsize);
}
