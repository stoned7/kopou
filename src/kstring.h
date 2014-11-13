#ifndef __KSTRING_H__
#define __KSTRING_H__

#include <stdlib.h>
#include <string.h>

#define lsize (sizeof(size_t))
typedef char* kstr_t;

kstr_t kstr_new(char *str);
kstr_t kstr_empty_new();
void kstr_del(kstr_t str);
kstr_t _kstr_create(char *str, size_t len);

kstr_t kstr_dup(kstr_t str);
kstr_t kstr_ncat_str(kstr_t kstr, const char *str, size_t len);
int kstr_tok_len(char *str, size_t len, const char *deli, kstr_t **tokens);
int kstr_tok(kstr_t str, const char *deli, kstr_t **tokens);
int kstr_cmp(const kstr_t str1, const kstr_t str2);
int kstr_tolower(kstr_t str);
int kstr_toupper(kstr_t str);

static inline size_t kstr_len(kstr_t str)
{
	//return *(size_t*)(str - lsize);
	return strlen(str);
}

#endif
