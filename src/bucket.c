#include "kopou.h"

typedef struct {
	kstr_t content_type;
	void *data;
	size_t size;
} bucket_obj_t;


int bucket_put_cmd(kconnection_t *c)
{
	khttp_request_t *r;
	kstr_t k;
	bucket_obj_t *o, *oo;
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
	memcpy(o->data, r->buf->next, o->size);

	if (kdb_exist(bucketdb, k)) {
		re = kdb_upd(bucketdb, k, o, (void**)&oo);
		if (re == K_OK) {
			kstr_del(oo->content_type);
			xfree(oo->data);
			xfree(oo);
		}
		kstr_del(k);
		reply_200(c);
		return K_OK;
	}

	kdb_add(bucketdb, k, o);
	reply_201(c);
	return K_OK;
}

int bucket_head_cmd(kconnection_t *c)
{
	reply_200(c);
	return K_OK;
}

int bucket_get_cmd(kconnection_t *c)
{
	kstr_t k;
	khttp_request_t *r;
	bucket_obj_t *o;
	kbuffer_t *b;
	char *p;
	char cl[64];
	char ct[128];
	size_t s, fs;

	r = c->req;
	k = r->cmd->params[0];

	o = kdb_get(bucketdb, k);
	if (!o) {
		reply_404(c);
		return K_OK;
	}

	snprintf(cl, 64, HTTP_H_CONTENTLENGTH_FMT, o->size);
	snprintf(ct, 128, HTTP_H_CONTENTTYPE_FMT, o->content_type);

	r->res->headers = xcalloc(2, sizeof(kstr_t));
	r->res->headers[0] = kstr_new(ct);
	r->res->headers[1] = kstr_new(cl);
	r->res->nheaders = 2;
	r->res->flag |= HTTP_RES_LENGTH;

	reply_200(c);

	b = r->res->buf->next;
	if (b == NULL)
		b = xmalloc(sizeof(kbuffer_t));
	s = o->size;

	if (s < HTTP_RES_WRITTEN_SIZE_MAX) {
		b->start = xmalloc(s);
		b->end = b->last = b->start + s -1;
		b->pos = b->start;
		b->next = NULL;
		memcpy(b->start, o->data, s);
		r->res->buf->next = b;
	} else {
		p = o->data;
		fs = HTTP_RES_WRITTEN_SIZE_MAX;
		do {
			b->start = xmalloc(fs);
			b->end = b->last = b->start + fs -1;
			b->pos =  b->start;
			memcpy(b->start, p, fs);
			p = p + fs;
			fs = s - fs;
			b = b->next = NULL;
		} while (fs > 0);
	}

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

	kstr_del(o->content_type);
	xfree(o->data);
	xfree(o);

	reply_200(c);
	return K_OK;
}

int stats_get_cmd(kconnection_t *c)
{
	char statsstr[512], cl[128];
	khttp_request_t *r = c->req;

	static char stats_format[] = "{\"objects\":%d, \"missed\":%d, \"hits\":%zu, \"deleted\":%zu, \"memused\": \"%zu\"}";
	snprintf(statsstr, 512, stats_format, stats.objects, stats.missed,
			stats.hits, stats.deleted, xalloc_total_mem_used());
	size_t len = strlen(statsstr);

	snprintf(cl, 128, HTTP_H_CONTENTLENGTH_FMT, len);
	r->res->headers = xcalloc(2, sizeof(kstr_t));
	r->res->headers[0] = kstr_new(HTTP_H_CONTENTTYPE_JSON);
	r->res->headers[1] = kstr_new(cl);
	r->res->nheaders = 2;
	r->res->flag |= HTTP_RES_LENGTH;

	reply_200(c);

	/* assumtion, response headers and content fit in same buffer 2KB*/
	kbuffer_t *b = r->res->curbuf;
	memcpy(b->last + 1, statsstr, len);
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
