#ifndef __AARRAY_H__
#define __AARRAY_H__

#include <stdint.h>

#include "common.h"
#include "kstring.h"
#include "xalloc.h"

#define AA_OK 0
#define AA_ERR -1
#define AA_UPDATE 1

typedef uint32_t (*_hashfunction)(const kstr_t key);
typedef int (*_keycomparer)(const kstr_t key1, const kstr_t key2);
typedef void (*aarray_del_handler)(kstr_t key, void *data);

typedef struct aarray_element {
	struct aarray_element *next;
	void *data;
	kstr_t key;
} aarray_element_t;


typedef struct aarray {
	aarray_element_t **buckets;
	_hashfunction hf;
	_keycomparer kc;
	aarray_del_handler adh;
	unsigned long nbucket;
	unsigned long mask;
	unsigned long nelement;
} aarray_t;

aarray_t *aarray_new(unsigned long size, _hashfunction hf,
			_keycomparer kc, aarray_del_handler adh);
void aarray_del(aarray_t *aa);

int aarray_add(aarray_t *aa, const kstr_t key, void *data, void **odata);
int aarray_rem(aarray_t *aa, const kstr_t key, void **data);
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
