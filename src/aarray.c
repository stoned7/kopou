#include <stdlib.h>
#include "aarray.h"

aarray_t *aarray_new(unsigned long size, aarray_hashfunction hf,
			aarray_key_comparer kc, aarray_ele_del_handler adh,
			aarray_ele_update_handler auh)
{
	aarray_t *aa = xmalloc(sizeof(*aa));
	aa->nbucket = size;
	aa->mask = size - 1;
	aa->nelement = 0;
	aa->hf = hf;
	aa->kc = kc;
	aa->adh = adh;
	aa->auh = auh;
	aa->buckets = xcalloc(size, sizeof(aarray_element_t*));
	return aa;
}

void aarray_del(aarray_t *aa)
{
	unsigned long i;
	aarray_element_t *ele, *temp;

	for (i = 0; i < aa->nbucket; i++) {
		ele = aa->buckets[i];
		while (ele) {
			if (aa->adh)
				aa->adh(ele->key, ele->data);
			temp = ele->next;
			xfree(ele);
			ele = temp;
		}
	}
	xfree(aa->buckets);
	xfree(aa);
}

static aarray_element_t *_aarray_find(aarray_t *aa, const kstr_t key)
{
	uint32_t hkey;
	unsigned long index;
	int cr;
	aarray_element_t *ele;

	hkey = aa->hf(key);
	index = hkey & aa->mask;

	ele = aa->buckets[index];
	while (ele) {
		cr = aa->kc(key, ele->key);
		if (!cr)
			return ele;
		ele = ele->next;
	}
	return NULL;
}

void *aarray_find(aarray_t *aa, const kstr_t key)
{
	aarray_element_t *ele = _aarray_find(aa, key);
	if (ele)
		return aarray_element_data(ele);
	return NULL;
}

int aarray_add(aarray_t *aa, const kstr_t key, void *data)
{
	uint32_t hkey;
	unsigned long index;
	aarray_element_t *ele;

	ele  = _aarray_find(aa, key);
	if (ele) {
		if (aa->auh)
			aa->auh(ele->data);
		ele->data = data;
	} else {
		hkey = aa->hf(key);
		index = hkey & aa->mask;
		ele = xmalloc(sizeof(aarray_element_t));
		ele->data = data;
		ele->key = key;
		ele->next = aa->buckets[index];
		aa->buckets[index] = ele;
		aa->nelement++;
	}
	return AA_OK;
}

int aarray_rem(aarray_t *aa, const kstr_t key, kstr_t *rkey, void **data)
{
	uint32_t hkey;
	unsigned long index;
	int cr;
	aarray_element_t *ele, *prev;

	hkey = aa->hf(key);
	index = hkey & aa->mask;

	prev = NULL;
	ele = aa->buckets[index];
	while (ele) {
		cr = aa->kc(key, ele->key);
		if (!cr) {
			if (prev == NULL) {
				aa->buckets[index] = ele->next;
				*data = ele->data;
				*rkey = ele->key;
			} else {
				prev->next = ele->next;
				*data = ele->data;
				*rkey = ele->key;
			}
			xfree(ele);
			aa->nelement--;
			return AA_OK;
		}
		prev = ele;
		ele = ele->next;
	}
	return AA_ERR;
}
