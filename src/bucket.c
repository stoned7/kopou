#include "kopou.h"

typedef struct {
	kstr_t content_type;
	void *data;
	size_t size;
	unsigned long long version;
} bucket_obj_t;

int bucket_put_cmd(kconnection_t *c)
{
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
	khttp_request_t *r = c->req;
	int i;
	for (i = 0; i < r->cmd->nparams; i++) {
		printf("param: %s\n", r->cmd->params[i]);
	}
	reply_200(c);
	return K_OK;
}

int bucket_delete_cmd(kconnection_t *c)
{
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
