#include "kopou.h"

typedef struct {
	kstr_t content_type;
	void *data;
	size_t size;
} bucket_obj_t;

char stats_format[] = "{\"objects\":%zu, \"bytes\":%zu, \"missed\":%zu, \"hits\":%zu, \"deleted\":%zu}";

int bucket_put_cmd(kconnection_t *c)
{
	khttp_request_t *r;
	kstr_t k;
	bucket_obj_t *o, *oo;
	kbuffer_t *b;
	size_t s, cs;
	int re;

	r = c->req;
	if (r->content_length < 1){
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

	if (kdb_exist(bucketdb, k)) {
		re = kdb_upd(bucketdb, k, o, (void**)&oo);
		if (re == K_OK) {
			kstr_del(oo->content_type);
			stats.space -= oo->size;
			xfree(oo->data);
			xfree(oo);
		}
		kstr_del(k);
		stats.space += o->size;
		reply_200(c);
		return K_OK;
	}

	kdb_add(bucketdb, k, o);
	stats.space += o->size;
	reply_201(c);
	return K_OK;
}

int bucket_head_cmd(kconnection_t *c)
{
	kstr_t k;
	khttp_request_t *r;
	bucket_obj_t *o;

	r = c->req;
	k = r->cmd->params[0];

	o = kdb_get(bucketdb, k);
	if (!o) {
		reply_404(c);
		return K_OK;
	}

	reply_200(c);
	return K_OK;
}

int bucket_get_cmd(kconnection_t *c)
{
	kstr_t k;
	khttp_request_t *r;
	bucket_obj_t *o;
	kbuffer_t *b;
	char cl[64], ct[128];

	r = c->req;
	k = r->cmd->params[0];

	o = kdb_get(bucketdb, k);
	if (!o) {
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

	return K_OK;
}

int bucket_delete_cmd(kconnection_t *c)
{
	kstr_t k;
	khttp_request_t *r;
	bucket_obj_t *o = NULL;

	r = c->req;
	k = r->cmd->params[0];

	if (kdb_rem(bucketdb, k, (void**)&o) == K_ERR) {
		reply_404(c);
		return K_OK;
	}

	stats.space -= o->size;
	kstr_del(o->content_type);
	xfree(o->data);
	xfree(o);

	reply_200(c);
	return K_OK;
}

int stats_get_cmd(kconnection_t *c)
{
	khttp_request_t *r;
	kbuffer_t *b;

	char statsstr[512], cl[128];
	size_t len;

	r = c->req;
	snprintf(statsstr, 512, stats_format, stats.objects, stats.space,
			stats.missed, stats.hits, stats.deleted);
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

int favicon_get_cmd(kconnection_t *c)
{
	khttp_request_t *r = c->req;
	r->res->flag |= HTTP_RES_CACHABLE;
	reply_200(c);
	return K_OK;
}
