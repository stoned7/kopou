#include "map.h"

keyval_t *keyval_new(kstr_t key, void *val)
{
	keyval_t *new = xmalloc(sizeof(keyval_t));
	new->key = key;
	new->val = val;
	return new;
}

void keyval_del(keyval_t *kv)
{
	xfree(kv);
}


map_t *map_new (map_delete_handler delete_handler)
{
	map_t *new = xmalloc(sizeof(map_t));
	new->dh = delete_handler;
	new->size = 0;
	new->pairs = NULL;
	return new;
}

void map_del(map_t *m)
{
	size_t i;
	for (i = 0; i < m->size; i++) {
		if (m->dh)
			m->dh(m->pairs[i]->key, m->pairs[i]->val);
		keyval_del(m->pairs[i]);
	}
	xfree(m->pairs);
	xfree(m);
}

int map_add(map_t *m, kstr_t key, void *val)
{
	m->size++;
	m->pairs = xrealloc(m->pairs, sizeof(keyval_t*) * m->size);
	m->pairs[m->size - 1] = keyval_new(key, val);
	return M_OK;
}

void *map_lookup(const map_t *m, const char *key)
{
	size_t i;
	keyval_t *kv;
	for (i = 0; i < m->size; i++) {
		kv = m->pairs[i];
		if (keyval_match(m->pairs[i]->key, key))
			return m->pairs[i]->val;
	}
	return NULL;
}
