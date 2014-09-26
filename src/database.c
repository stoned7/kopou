#include "kopou.h"

kopou_db_t *kdb_new(unsigned long size, int loadfactor, _hashfunction hf,
		_keycomparer kc)
{
	kopou_db_t *db = xmalloc(sizeof(kopou_db_t));
	db->main = xmalloc(sizeof(_kopou_db_t));
	db->main->primary = aarray_new(size, hf, kc, NULL);
	if (!db->main->primary)
		_kdie("primary db creation fail");
	db->main->secondary = NULL;
	db->main->flag = KDB_FLAG_NONE;
	db->main->loadfactor = loadfactor;
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

void *kdb_get(kopou_db_t *db, kstr_t key)
{
	K_FORCE_USE(db);
	K_FORCE_USE(key);
	return NULL;
}

int kdb_add(kopou_db_t *db, kstr_t key, void *obj, void **oobj)
{
	K_FORCE_USE(db);
	K_FORCE_USE(key);
	K_FORCE_USE(obj);
	K_FORCE_USE(oobj);
	return K_OK;
}

int kdb_rem(kopou_db_t *db, kstr_t key, void **obj)
{
	K_FORCE_USE(db);
	K_FORCE_USE(key);
	K_FORCE_USE(obj);
	return K_OK;
}
