#include "kopou.h"

kopou_db_t *kdb_new(int id, unsigned long size, int loadfactor,
			_hashfunction hf, _keycomparer kc)
{
	kopou_db_t *db = xmalloc(sizeof(kopou_db_t));
	db->main = xmalloc(sizeof(_kopou_db_t));
	db->main->primary = aarray_new(size, hf, kc, NULL);
	if (!db->main->primary)
		_kdie("primary db creation fail");
	db->main->secondary = NULL;
	db->main->loadfactor = loadfactor;
	db->main->rehashpos = -1;
	db->dirty = 0;
	db->enable_resize = 1;
	db->id = id;
	return db;
}

void kdb_del(kopou_db_t *db)
{
	if (db->main->secondary)
		aarray_del(db->main->secondary);
	aarray_del(db->main->primary);
	xfree(db->main);
	xfree(db);
}

static inline int _rehashing(kopou_db_t *db)
{
	return (db->main->rehashpos != -1);
}

void *kdb_get(kopou_db_t *db, kstr_t key)
{
	void *o;

	o = aarray_find(db->main->primary, key);
	if (!o && _rehashing(db))
		o = aarray_find(db->main->secondary, key);

	if (o == NULL) stats.missed++;
	else stats.hits++;

	return o;
}

int kdb_exist(kopou_db_t *db, kstr_t key)
{
	int r;
	r = aarray_exist(db->main->primary, key);
	if (!r && _rehashing(db))
		r = aarray_exist(db->main->secondary, key);
	return r;
}

int kdb_add(kopou_db_t *db, kstr_t key, void *obj)
{
	int r;

	if (_rehashing(db))
		r = aarray_add(db->main->secondary, key, obj);
	else
		r = aarray_add(db->main->primary, key, obj);

	if (r == AA_OK) {
		db->dirty++;
		stats.objects++;
	}
	return r;
}

int kdb_upd(kopou_db_t *db, kstr_t key, void *obj, void **oobj)
{
	int r;
	r = aarray_upd(db->main->primary, key, obj, oobj);
	if (r == AA_ERR && _rehashing(db)) {
		r = aarray_upd(db->main->secondary, key, obj, oobj);
	}

	if (r == AA_OK) db->dirty++;
	return r;
}

int kdb_rem(kopou_db_t *db, kstr_t key, void **obj)
{
	int r;
	r = aarray_rem(db->main->primary, key, obj);
	if (r == AA_ERR && _rehashing(db))
		r = aarray_rem(db->main->primary, key, obj);

	if (r == AA_OK) {
		db->dirty++;
		stats.objects--;
	}
	return r;
}

static int _expand(kopou_db_t *db)
{
	uint32_t hkey;
	unsigned long index;
	aarray_element_t *ele, *tmp;

	ele = db->main->primary->buckets[db->main->rehashpos];
	while (ele) {
		hkey = db->main->secondary->hf(ele->key);
		index = hkey & db->main->secondary->mask;
		tmp = ele->next;
		ele->next = db->main->secondary->buckets[index];
		db->main->secondary->buckets[index] = ele;
		ele = tmp;
	}
	db->main->rehashpos++;

	if (db->main->rehashpos >= (long)aarray_size(db->main->primary)) {
		db->main->rehashpos = -1;
		aarray_fdel(db->main->primary);
		db->main->primary = db->main->secondary;
		db->main->secondary = NULL;
	}

	return K_OK;
}

int kdb_try_expand(kopou_db_t *db)
{
	int f;
	unsigned long c, n, t;

	if (!db->enable_resize) return K_AGAIN;

	if (_rehashing(db))
		return _expand(db);

	c = aarray_size(db->main->primary);
	t = aarray_total_elements(db->main->primary);

	if (c > t) return K_AGAIN;

	f = t / c;
	if (f <= db->main->loadfactor) return K_AGAIN;

	if (c >= LONG_MAX) n = LONG_MAX;
	else n = c << 1;

	db->main->secondary = aarray_new(n, db->main->primary->hf,
					db->main->primary->kc, NULL);
	db->main->rehashpos = 0;

	return _expand(db);
}


