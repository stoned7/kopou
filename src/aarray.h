#ifndef __AARRAY_H__
#define __AARRAY_H__

#include <stdint.h>

#include "kstring.h"
#include "xalloc.h"

#define AA_DEFAULT_SIZE 1024
#define AA_OK 0
#define AA_ERR -1

typedef uint32_t (*aarray_hashfunction)(const kstr_t key);
typedef int (*aarray_key_comparer)(const kstr_t key1, const kstr_t key2);
typedef void (*aarray_ele_del_handler)(kstr_t key, void *data);
typedef void (*aarray_ele_update_handler)(void *data);

typedef struct aarray_element {
	void *data;
	kstr_t key;
	struct aarray_element *next;
} aarray_element_t;


typedef struct aarray {
	aarray_element_t **buckets;
	unsigned long nbucket;
	unsigned long mask;
	unsigned long nelement;
	aarray_hashfunction hf;
	aarray_key_comparer kc;
	aarray_ele_del_handler adh;
	aarray_ele_update_handler auh;
} aarray_t;

aarray_t *aarray_new(unsigned long size, aarray_hashfunction hf,
			aarray_key_comparer kc, aarray_ele_del_handler adh,
			aarray_ele_update_handler auh);
void aarray_del(aarray_t *aa);

int aarray_add(aarray_t *aa, const kstr_t key, void *data);
int aarray_rem(aarray_t *aa, const kstr_t key, kstr_t *rkey, void **data);
void *aarray_find(aarray_t *aa, const kstr_t key);

static inline unsigned long aarray_size(const aarray_t *aa)
{
	return aa->nbucket;
}

static inline unsigned long aarray_total_elements(const aarray_t *aa)
{
	return aa->nelement;
}

static inline void *aarray_element_data(const aarray_element_t *ele)
{
	return ele->data;
}

static inline kstr_t aarray_element_key(const aarray_element_t *ele)
{
	return ele->key;
}

#endif