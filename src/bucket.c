#include "kopou.h"

#define BUCKET_ITEM_VALID (1)
#define BUCKET_ITEM_INVALID (0)

#define BUCKET_SIZE_LEN8 (8)
#define BUCKET_SIZE_LEN16 (16)
#define BUCKET_SIZE_LEN24 (24)
#define BUCKET_SIZE_LEN32 (32)

typedef struct {
	kstr_t content_type;
	void *data;
	size_t size;
} bucket_obj_t;

char stats_format[] = "{\"objects\":%zu, \"bytes\":%zu, \"missed\":%zu, \"hits\":%zu, \"deleted\":%zu}";

int bucket_backup_objects(FILE *fp, kopou_db_t *db)
{
	kopou_db_iter_t *iter;
	kstr_t key;
	bucket_obj_t *obj;

	iter = kdb_iter_new(db);

	while (kdb_iter_foreach(iter, &key, (void**)&obj)) {

	}

	kdb_iter_del(iter);
	return K_OK;
}


/* this routine run async, care about cow */
static int bucket_backup_hdd(kopou_db_t *db)
{
	FILE *fp;
	int r;
	int16_t dbid, dbv;
	int64_t total;
	char *signature = "kopou";

	fp = fopen("bucket.tmp", "w");
	if (!fp)
		return K_ERR;

	r = bgs_write(fp, signature, 5);
	if  (r == 0)
		return K_ERR;

	dbid = htobe16(db->id);
	r = bgs_write(fp, &dbid, 2);
	if  (r == 0)
		return K_ERR;

	dbv = 1;
	dbv = htobe16(dbv);
	r = bgs_write(fp, &dbv, 2);
	if  (r == 0)
		return K_ERR;

	total = db->stats->objects;
	total = htobe64(total);
	r = bgs_write(fp, &total, 8);
	if  (r == 0)
		return K_ERR;

	fclose(fp);
	return K_OK;
}

kopou_db_t *bucket_init(int id)
{
	kopou_db_t *db;
	db = kdb_new(id, 16, 5, generic_hf, generic_kc);
	db->backup_hdd = bucket_backup_hdd;
	return db;
}

static int bucket_put_cmd(kconnection_t *c)
{
	khttp_request_t *r;
	kstr_t k;
	bucket_obj_t *o, *oo;
	kbuffer_t *b;
	kopou_db_t *bucketdb;

	size_t s, cs;
	int re;

	r = c->req;
	if (r->content_length < 1) {
		reply_411(c);
		return K_OK;
	}

	k = kstr_dup(r->cmd->params[0]);
	o = xmalloc(sizeof(bucket_obj_t));
	o->content_type = kstr_dup(r->content_type);
	o->size =  r->content_length;

	o->data = xmalloc(o->size);

	b = r->buf;
	s = b->last - (r->header_end +2);
	memcpy(o->data, (r->header_end +2), s);

	b = b->next;
	if (b) {
		cs = (b->last - b->start);
		memcpy(o->data + s, b->start, cs);
		s += cs;
		b = b->next;
	}

	bucketdb = get_db(r->cmd->db_id);

	if (kdb_exist(bucketdb, k)) {
		re = kdb_upd(bucketdb, k, o, (void**)&oo);
		if (re == K_OK) {
			kopou.space -= oo->size;
			bucketdb->stats->space -= oo->size;

			kstr_del(oo->content_type);
			xfree(oo->data);
			xfree(oo);
		}

		bucketdb->dirty++;
		bucketdb->stats->space += o->size;
		kopou.space += o->size;

		kstr_del(k);
		reply_200(c);
		return K_OK;
	}

	kdb_add(bucketdb, k, o);

	bucketdb->dirty++;
	bucketdb->stats->objects++;
	bucketdb->stats->space += o->size;
	kopou.space += o->size;

	reply_201(c);
	return K_OK;
}

static int bucket_head_cmd(kconnection_t *c)
{
	kstr_t k;
	khttp_request_t *r;
	bucket_obj_t *o;
	kopou_db_t *bucketdb;

	r = c->req;
	k = r->cmd->params[0];

	bucketdb = get_db(r->cmd->db_id);

	o = kdb_get(bucketdb, k);
	if (!o) {
		bucketdb->stats->missed++;
		reply_404(c);
		return K_OK;
	}

	bucketdb->stats->hits++;
	reply_200(c);
	return K_OK;
}

static int bucket_get_cmd(kconnection_t *c)
{
	kstr_t k;
	khttp_request_t *r;
	bucket_obj_t *o;
	kbuffer_t *b;
	kopou_db_t *bucketdb;
	char cl[64], ct[128];

	r = c->req;
	k = r->cmd->params[0];
	bucketdb = get_db(r->cmd->db_id);

	o = kdb_get(bucketdb, k);
	if (!o) {
		bucketdb->stats->missed++;
		reply_404(c);
		return K_OK;
	}

	r->res->size_hint += o->size;
	snprintf(cl, 64, HTTP_H_CONTENTLENGTH_FMT, o->size);
	snprintf(ct, 128, HTTP_H_CONTENTTYPE_FMT, o->content_type);

	r->res->headers = xcalloc(2, sizeof(kstr_t));
	r->res->headers[0] = kstr_new(ct);
	r->res->headers[1] = kstr_new(cl);
	r->res->nheaders = 2;
	r->res->flag |= HTTP_RES_LENGTH;

	reply_200(c);

	b = r->res->curbuf;
	memcpy(b->last, o->data, o->size);
	b->last += o->size;

	bucketdb->stats->hits++;
	return K_OK;
}

static int bucket_delete_cmd(kconnection_t *c)
{
	kstr_t k;
	khttp_request_t *r;
	bucket_obj_t *o = NULL;
	kopou_db_t *bucketdb;

	r = c->req;
	k = r->cmd->params[0];
	bucketdb = get_db(r->cmd->db_id);

	if (kdb_rem(bucketdb, k, (void**)&o) == K_ERR) {
		reply_404(c);
		return K_OK;
	}

	bucketdb->dirty++;
	bucketdb->stats->objects--;
	bucketdb->stats->space -= o->size;
	kopou.space -= o->size;

	kstr_del(o->content_type);
	xfree(o->data);
	xfree(o);

	reply_200(c);
	return K_OK;
}

static int bucket_stats_get_cmd(kconnection_t *c)
{
	khttp_request_t *r;
	kbuffer_t *b;
	kopou_db_t *bucketdb;

	char statsstr[512], cl[128];
	size_t len;

	r = c->req;
	bucketdb = get_db(r->cmd->db_id);

	snprintf(statsstr, 512, stats_format, bucketdb->stats->objects,
		bucketdb->stats->space, bucketdb->stats->missed,
		bucketdb->stats->hits, bucketdb->stats->deleted);
	len = strlen(statsstr);
	r->res->size_hint += len;

	snprintf(cl, 128, HTTP_H_CONTENTLENGTH_FMT, len);
	r->res->headers = xcalloc(2, sizeof(kstr_t));
	r->res->headers[0] = kstr_new(HTTP_H_CONTENTTYPE_JSON);
	r->res->headers[1] = kstr_new(cl);
	r->res->nheaders = 2;
	r->res->flag |= HTTP_RES_LENGTH;

	reply_200(c);

	b = r->res->curbuf;
	memcpy(b->last, statsstr, len);
	b->last = b->last + len;

	return K_OK;
}

static int favicon_get_cmd(kconnection_t *c)
{
	khttp_request_t *r = c->req;
	r->res->flag |= HTTP_RES_CACHABLE;
	reply_200(c);
	return K_OK;
}

void bucket_init_cmds_table(int db_id)
{
	unsigned nt, p;
	kstr_t bucket_name, stats_name, favicon_name;
	kcommand_t *headbucket, *getbucket, *putbucket, *delbucket, *statsbucket;
	kcommand_t *favicon;

	/* bucket */
	bucket_name = kstr_new("bucket");
	nt = 2;
	p = 1;
	headbucket = xmalloc(sizeof(kcommand_t));
	*headbucket = (kcommand_t){ .method = HTTP_METHOD_HEAD, .db_id = db_id,
		.execute = bucket_head_cmd, .nptemplate = nt, .nparams = p,
		.flag = KCMD_READ_ONLY | KCMD_SKIP_REQUEST_BODY |
			KCMD_SKIP_REPLICA | KCMD_SKIP_PERSIST };
	headbucket->ptemplate = xcalloc(nt, sizeof(kstr_t));
	headbucket->ptemplate[0] = bucket_name;
	headbucket->ptemplate[1] = NULL;
	headbucket->params = xcalloc(p, sizeof(kstr_t));

	getbucket = xmalloc(sizeof(kcommand_t));
	*getbucket = (kcommand_t){ .method = HTTP_METHOD_GET, .db_id = db_id,
		.execute = bucket_get_cmd, .nptemplate = nt, .nparams = p,
		.flag = KCMD_READ_ONLY | KCMD_SKIP_REQUEST_BODY |
			KCMD_SKIP_REPLICA | KCMD_SKIP_PERSIST };
	getbucket->ptemplate = xcalloc(nt, sizeof(kstr_t));
	getbucket->ptemplate[0] = bucket_name;
	getbucket->ptemplate[1] = NULL;
	getbucket->params = xcalloc(p, sizeof(kstr_t));
	headbucket->next = getbucket;

	putbucket = xmalloc(sizeof(kcommand_t));
	*putbucket = (kcommand_t){ .method = HTTP_METHOD_PUT, .db_id = db_id,
		.execute = bucket_put_cmd, .nptemplate = nt, .nparams = p,
		.flag = KCMD_WRITE_ONLY };
	putbucket->ptemplate = xcalloc(nt, sizeof(kstr_t));
	putbucket->ptemplate[0] = bucket_name;
	putbucket->ptemplate[1] = NULL;
	putbucket->params = xcalloc(p, sizeof(kstr_t));
	getbucket->next = putbucket;

	delbucket = xmalloc(sizeof(kcommand_t));
	*delbucket = (kcommand_t){ .method = HTTP_METHOD_DELETE,
		.execute = bucket_delete_cmd, .nptemplate = nt, .nparams = p,
		.db_id = db_id,
		.flag = KCMD_WRITE_ONLY | KCMD_SKIP_REQUEST_BODY };
	delbucket->ptemplate = xcalloc(nt, sizeof(kstr_t));
	delbucket->ptemplate[0] = bucket_name;
	delbucket->ptemplate[1] = NULL;
	delbucket->params = xcalloc(p, sizeof(kstr_t));
	putbucket->next = delbucket;

	stats_name = kstr_new("stats");
	nt = 2;
	p = 0;
	statsbucket = xmalloc(sizeof(kcommand_t));
	*statsbucket = (kcommand_t){ .method = HTTP_METHOD_GET, .next = NULL,
		.db_id = db_id, .execute = bucket_stats_get_cmd,
		.nptemplate = nt, .nparams = p,
		.flag = KCMD_READ_ONLY | KCMD_SKIP_REQUEST_BODY |
			KCMD_SKIP_REPLICA | KCMD_SKIP_PERSIST };
	statsbucket->ptemplate = xcalloc(nt, sizeof(kstr_t));
	statsbucket->ptemplate[0] = bucket_name;
	statsbucket->ptemplate[1] = stats_name;
	delbucket->next = statsbucket;

	map_add(cmds_table, bucket_name, headbucket);

	/* favicon */
	favicon_name = kstr_new("favicon.ico");
	nt = 1;
	p = 0;
	favicon = xmalloc(sizeof(kcommand_t));
	*favicon = (kcommand_t){ .method = HTTP_METHOD_GET, .nparams = p,
		.db_id = -1, .execute = favicon_get_cmd, .next = NULL,
		.nptemplate = nt,
		.flag = KCMD_READ_ONLY | KCMD_SKIP_REQUEST_BODY |
		KCMD_SKIP_REPLICA | KCMD_SKIP_PERSIST | KCMD_RESPONSE_CACHABLE };
	favicon->ptemplate = xcalloc(nt, sizeof(kstr_t));
	favicon->ptemplate[0] = favicon_name;
	map_add(cmds_table, favicon_name, favicon);
}
