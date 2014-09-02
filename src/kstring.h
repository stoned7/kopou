#ifndef __KSTRING_H__
#define __KSTRING_H__

#include <stdlib.h>
#include <string.h>

#define lsize (sizeof(size_t))
typedef char* kstr_t;

kstr_t kstr_new(char *str, size_t len);
kstr_t kstr_empty_new()
void kstr_del(kstr_t str);

kstr_t kstr_cat_strn(kstr_t kstr, char *str, size_t len)

static inline size_t kstr_len(kstr_t str)
{
	return *(size_t*)(str - lsize);
}

#endif
