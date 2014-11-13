#include "kopou.h"

kopou_db_t *kdb_new(int id, unsigned long size, int loadfactor,
			_hashfunction hf, _keycomparer kc)
{
	kopou_db_t *db = xmalloc(sizeof(kopou_db_t));

	db->main = xmalloc(sizeof(struct kopou_db_s));
	db->main->primary = aarray_new(size, hf, kc, NULL);
	if (!db->main->primary)
		_kdie("primary db creation fail");
	db->main->secondary = NULL;
	db->main->loadfactor = loadfactor;
	db->main->rehashpos = -1;

	db->dirty = 0;
	db->dirty_considered = 0;
	db->id = id;
	db->resize_done_ts = kopou.current_time;
	db->backup_hdd = NULL;

	db->stats =  xmalloc(sizeof(kopou_db_stats_t));
	db->stats->objects = 0;
	db->stats->missed = 0;
	db->stats->hits = 0;
	db->stats->deleted = 0;
	db->stats->space = 0;

	return db;
}

void kdb_del(kopou_db_t *db)
{
	if (db->main->secondary)
		aarray_del(db->main->secondary);
	aarray_del(db->main->primary);
	xfree(db->main);
	xfree(db->stats);
	xfree(db);
}

static inline int kdb_rehashing(kopou_db_t *db)
{
	return (db->main->rehashpos != -1);
}

void *kdb_get(kopou_db_t *db, kstr_t key)
{
	void *o;

	o = aarray_find(db->main->primary, key);
	if (!o && kdb_rehashing(db))
		o = aarray_find(db->main->secondary, key);
	return o;
}

int kdb_exist(kopou_db_t *db, kstr_t key)
{
	int r;

	r = aarray_exist(db->main->primary, key);
	if (!r && kdb_rehashing(db))
		r = aarray_exist(db->main->secondary, key);
	return r;
}

int kdb_add(kopou_db_t *db, kstr_t key, void *obj)
{
	int r;

	if (kdb_rehashing(db))
		r = aarray_add(db->main->secondary, key, obj);
	else
		r = aarray_add(db->main->primary, key, obj);
	return r;
}

int kdb_upd(kopou_db_t *db, kstr_t key, void *obj, void **oobj)
{
	int r;

	r = aarray_upd(db->main->primary, key, obj, oobj);
	if (r == AA_ERR && kdb_rehashing(db))
		r = aarray_upd(db->main->secondary, key, obj, oobj);
	return r;
}

int kdb_rem(kopou_db_t *db, kstr_t key, void **obj)
{
	int r;

	r = aarray_rem(db->main->primary, key, obj);
	if (r == AA_ERR && kdb_rehashing(db))
		r = aarray_rem(db->main->primary, key, obj);
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
		db->resize_done_ts = kopou.current_time;
	}

	return K_OK;
}

int kdb_try_expand(kopou_db_t *db)
{
	int f;
	unsigned long c, n, t;

	if (kdb_rehashing(db))
		return _expand(db);

	c = aarray_size(db->main->primary);
	if (c >= LONG_MAX)
		return K_AGAIN;

	t = aarray_total_elements(db->main->primary);
	if (c > t)
		return K_AGAIN;


	if (difftime(kopou.current_time, db->resize_done_ts)
		< settings.db_resize_interval)
		return K_AGAIN;

	f = t / c;
	if (f <= db->main->loadfactor)
		return K_AGAIN;
	do {
		n = c << 1;
		if (n > LONG_MAX) {
			n = LONG_MAX;
			break;
		}
		f = t/n;
		c = n;
	} while (f > db->main->loadfactor);

	db->main->secondary = aarray_new(n, db->main->primary->hf,
					db->main->primary->kc, NULL);
	db->main->rehashpos = 0;
	return _expand(db);
}

kopou_db_iter_t *kdb_iter_new(kopou_db_t *db)
{
	kopou_db_iter_t *it = xmalloc(sizeof(kopou_db_iter_t));
	it->db = db;
	it->index = -1;
	it->aa = db->main->primary;
	it->cur = NULL;
	it->jumped = 0;
	return it;
}

void kdb_iter_reset(kopou_db_iter_t *it, kopou_db_t *db)
{
	it->db = db;
	it->index = -1;
	it->aa = db->main->primary;
	it->cur = NULL;
	it->jumped = 0;
}

void kdb_iter_del(kopou_db_iter_t *it)
{
	xfree(it);
}

static aarray_element_t *kdb_iter_get_next_index(kopou_db_iter_t *it)
{
	aarray_element_t *cur;

	it->index += 1;
	while (it->index < aarray_size(it->aa)) {
		cur = it->aa->buckets[it->index];
		if (cur)
			return cur;
		it->index += 1;
	}

	if (!it->jumped && it->db->main->secondary) {
		it->jumped = 1;
		it->aa = it->db->main->secondary;
		it->index = 0;
		while (it->index < aarray_size(it->aa)) {
			cur = it->aa->buckets[it->index];
			if (cur)
				return cur;
			it->index += 1;
		}
	}
	return NULL;
}


int kdb_iter_foreach(kopou_db_iter_t *it, kstr_t *k, void **o)
{
	aarray_element_t *cur;

	cur = it->cur;
	if (cur == NULL) {
		cur = kdb_iter_get_next_index(it);
		it->cur = cur;
		if (cur) {
			*k = aarray_element_key(cur);
			*o = aarray_element_data(cur);
			return 1;
		}
		return 0;
	}

	cur = aarray_element_next(cur);
	if (cur) {
		it->cur = cur;
		*k = aarray_element_key(cur);
		*o = aarray_element_data(cur);
		return 1;
	}

	cur = kdb_iter_get_next_index(it);
	it->cur = cur;
	if (cur) {
		*k = aarray_element_key(cur);
		*o = aarray_element_data(cur);
		return 1;
	}
	return 0;
}

