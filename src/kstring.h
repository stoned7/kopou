#ifndef __KSTRING_H__
#define __KSTRING_H__

#include <stdlib.h>
#include <string.h>

typedef char* kstr_t;

kstr_t kstr_len_new(char *str, size_t len);
kstr_t kstr_new(char *str);
void kstr_del(kstr_t str);

size_t kstr_len(kstr_t str);

#endif
