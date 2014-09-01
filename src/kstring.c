#include "kstring.h"
#include "xalloc.h"

#define LEN_SIZE (sizeof(size_t))

kstr_t kstr_len_new(char *str, size_t len)
{
	kstr_t kstr = xmalloc(LEN_SIZE + len + 1);
	memcpy(kstr, &len, LEN_SIZE);
	memcpy(kstr + LEN_SIZE, str, len);
	kstr[LEN_SIZE + len] = '\0';
	return kstr + LEN_SIZE;
}

kstr_t kstr_new(char *str)
{
	return kstr_len_new(str, strlen(str));
}

void kstr_del(kstr_t str)
{
	xfree(str - LEN_SIZE);
	str = NULL;
}

size_t kstr_len(kstr_t str)
{
	return *(size_t*)(str - LEN_SIZE);
}
