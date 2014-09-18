#include "kopou.h"

#define CRLF "\r\n"
#define HTTP_RESLINE_200 "HTTP/1.1 200 OK\r\n"
#define HTTP_H_CONNECTION_KEEPALIVE "Connection: keep-alive\r\n"
#define HTTP_H_CONNECTION_CLOSE "Connection: close\r\n"
#define HTTP_H_NO_CACHE "Cache-Control: no-cache, no-store, must-revalidate\r\n"
#define HTTP_H_YES_CACHE "Cache-Control: public, max-age=315360000\r\n"
#define HTTP_H_CONTENTLENGTH "Content-Length: %zu\r\n"
#define HTTP_H_CONTENTTYPE "Content-Type: %s\r\n"
#define HTTP_H_ETAG "Etag: \"%s\"\r\n";

int reply_400(kconnection_t *c)
{
	K_FORCE_USE(c);
	return K_OK;
}

int reply_413(kconnection_t *c)
{
	K_FORCE_USE(c);
	return K_OK;
}

int reply_404(kconnection_t *c)
{
	K_FORCE_USE(c);
	return K_OK;
}

int reply_500(kconnection_t *c)
{
	K_FORCE_USE(c);
	return K_OK;
}

int reply_200(kconnection_t *c)
{
	khttp_request_t *r = c->req;
	kbuffer_t *b = r->res->curbuf;
	if (b == NULL) {
		b = xmalloc(sizeof(kbuffer_t));
		b->start = xmalloc(1024 << 2);
		b->end = b->start + (1024 << 2) - 1;
		b->pos = b->last = b->start;
		b->next = NULL;
		r->res->buf = r->res->curbuf = b;

		char *rl = HTTP_RESLINE_200;
		memcpy(b->pos, rl, strlen(rl));
		b->last = b->pos + strlen(rl);


	}

	return K_OK;
}

int reply_300(kconnection_t *c)
{
	K_FORCE_USE(c);
	return K_OK;
}
