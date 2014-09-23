#ifndef __MAP_H__
#define __MAP_H__

#include <string.h>
#include "xalloc.h"
#include "kstring.h"

#define M_OK 0
#define M_ERR -1

typedef struct {
	kstr_t key;
	void *val;
} keyval_t;

keyval_t* keyval_new(kstr_t key, void *val);
void keyval_del(keyval_t *kv);
static inline int keyval_match(const char *key1, const char *key2)
{
	return !strcasecmp(key1, key2);
}

typedef void (*map_delete_handler)(kstr_t key, void *val);
typedef struct {
	keyval_t **pairs;
	map_delete_handler dh;
	size_t size;
} map_t;

map_t* map_new (map_delete_handler delete_handler);
void map_del(map_t *m);
int map_add(map_t *m, kstr_t key, void *val);
void* map_lookup(const map_t *m, const char *key);


#endif
