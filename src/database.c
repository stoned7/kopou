#include "kopou.h"

kopou_db_t *kdb_new(aarray_hashfunction hf, aarray_key_comparer kc)
{
	kopou_db_t *db = xmalloc(sizeof(kopou_db_t));
	db->main = xmalloc(sizeof(_kopou_db_t));
	db->main->primary = aarray_new(AA_DEFAULT_SIZE, hf, kc, NULL, NULL);
	if (!db->main->primary)
		_kdie("primary db creation fail");
	db->main->secondary = NULL;
	db->main->flag = KDB_FLAG_NONE;
	db->main->loadfactor = 5;
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

int kdb_add(kopou_db_t *db, kstr_t key, void *obj, void **oldobj)
{
	K_FORCE_USE(db);
	K_FORCE_USE(key);
	K_FORCE_USE(obj);
	K_FORCE_USE(oldobj);
	return K_OK;
}

int kdb_rem(kopou_db_t *db, kstr_t key, void **obj)
{
	K_FORCE_USE(db);
	K_FORCE_USE(key);
	K_FORCE_USE(obj);
	return K_OK;
}
