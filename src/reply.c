#include "kopou.h"

#define CRLF "\r\n"

#define HTTP_H_CONNECTION_KEEPALIVE "Connection: keep-alive\r\n"
#define HTTP_H_CONNECTION_CLOSE "Connection: close\r\n"
#define HTTP_H_YES_CACHE "Cache-Control: public, max-age=315360000\r\n"
#define HTTP_H_CONTENTLENGTH "Content-Length: %zu\r\n"
#define HTTP_H_CONTENTTYPE "Content-Type: %s\r\n"
#define HTTP_H_ETAG "Etag: \"%s\"\r\n";

#define HTTP_RESPONSE_CACHABLE 1 << 0
#define HTTP_RESPONSE_CHUNKED 1 << 1

static void set_response_http_headers(kconnection_t *c, kbuffer_t *b)
{
	int i;
	size_t s;
	kstr_t *h;
	char fh[128];

	khttp_request_t *r = c->req;

	for (i = 0; i < r->res->nheaders i++) {
		h = r->res->headers[i];
		s = kstr_len(h);
		memcpy(b->last + 1, h, s);
		b->last += s;
	}

	get_http_date(fh, 128);
	s = strlen(fh);
	memcpy(b->last + 1, fh, s);
	b->last += s;

	get_http_server_str(fh, 128);
	s = strlen(fh);
	memcpy(b->last + 1, fh, s);
	b->last += s;
}

static void reply_err(kconnection_t *c, char *rl, int rllen)
{
	size_t s;
	char *h;

	khttp_request_t *r = c->req;
	kbuffer_t *b = r->res->buf;
	if (b == NULL) {
		b = xmalloc(sizeof(kbuffer_t));
		b->start = xmalloc(1024 << 2);
		b->end = b->start + (1024 << 2) - 1;
		r->res->buf = r->res->curbuf = b;
		b->next = NULL;
	} else
		r->res->curbuf = r->res->buf;
	b->pos = b->last = b->start;

	memcpy(b->pos, rl, rllen);
	b->last = b->pos + rllen - 1;

	if (settings.http_close_connection_onerror || r->connection_close ||
			!r->connection_keepalive)
		h = HTTP_H_CONNECTION_CLOSE;
	else
		h = HTTP_H_CONNECTION_KEEPALIVE;
	s = strlen(h);
	memcpy(b->last + 1, h, s);
	b->last =+ s;

	h = "Cache-Control: no-cache, no-store, must-revalidate\r\n";
	s = strlen(ch);
	memcpy(b->last + 1, ch, s);
	b->last =+ s;

	set_response_http_headers(c, b);

	h = "Content-Length: 0\r\n\r\n";
	s = strlen(cl);
	memcpy(b->last + 1, cl, s);
	b->last =+ s;
}

void reply_400(kconnection_t *c)
{
	char rl[] = "HTTP/1.1 400 BadRequest\r\n";
	reply_err(c, rl, strlen(rl));
}

void reply_413(kconnection_t *c)
{
	char rl[] = "HTTP/1.1 413 RequestTooLarge\r\n";
	reply_err(c, rl, strlen(rl));
}

void reply_404(kconnection_t *c)
{
	char rl[] = "HTTP/1.1 404 NotFound\r\n";
	reply_err(c, rl, strlen(rl));
}

void reply_411(kconnection_t *c)
{
	char rl[] = "HTTP/1.1 411 LengthRequired\r\n";
	reply_err(c, rl, strlen(rl));
}

void reply_500(kconnection_t *c)
{
	char rl[] = "HTTP/1.1 500 ServerInternalError\r\n";
	reply_err(c, rl, strlen(rl));
}

void reply_501(kconnection_t *c)
{
	char rl[] = "HTTP/1.1 501 NotImplemented\r\n";
	reply_err(c, rl, strlen(rl));
}

void reply_503_now(kbuffer_t *b)
{
	char fh[128];
	char *h;

	char rl[] = "HTTP/1.1 503 ServerTooBusy\r\n";
	size_t s = strlen(rl);

	memcpy(b->pos, rl, s);
	b->last = b->pos + s - 1;

	h = HTTP_H_CONNECTION_CLOSE;
	s = strlen(h);
	memcpy(b->last + 1, h, s);
	b->last =+ s;

	h = "Cache-Control: no-cache, no-store, must-revalidate\r\n";
	s = strlen(ch);
	memcpy(b->last + 1, ch, s);
	b->last =+ s;

	get_http_date(fh, 128);
	s = strlen(fh);
	memcpy(b->last + 1, fh, s);
	b->last += s;

	get_http_server_str(fh, 128);
	s = strlen(fh);
	memcpy(b->last + 1, fh, s);
	b->last += s;

	h = "Content-Length: 0\r\n\r\n";
	s = strlen(cl);
	memcpy(b->last + 1, cl, s);
	b->last =+ s;
}

void reply_505(kconnection_t *c)
{
	char rl[] = "HTTP/1.1 505 VersionNotSupported\r\n";
	reply_err(c, rl, strlen(rl));
}

void reply_200(kconnection_t *c)
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

		size_t s;
		char *rl = "HTTP/1.1 200 OK\r\n";
		s = strlen(rl);
		memcpy(b->pos, rl, s);
		b->last = b->pos + s - 1;

		char *cn = HTTP_H_CONNECTION_KEEPALIVE;
		s = strlen(cn);
		memcpy(b->last + 1, cn, s);
		b->last = b->last + s;

		char *ch = "Cache-Control: no-cache, no-store, must-revalidate\r\n";
		s = strlen(ch);
		memcpy(b->last + 1, ch, s);
		b->last = b->last + s;

		char dt[128];
		get_http_date(dt, 128);
		s = strlen(dt);
		memcpy(b->last + 1, dt, s);
		b->last = b->last + s;

		char sv[128];
		get_http_server_str(sv, 128);
		s = strlen(sv);
		memcpy(b->last + 1, sv, s);
		b->last = b->last + s;

		char *cl = HTTP_H_CONTENTLENGTH_ZERO;
		s = strlen(cl);
		memcpy(b->last + 1, cl, s);
		b->last = b->last + s;

		char *end = CRLF;
		memcpy(b->last + 1,end, 2);
		b->last = b->last + 2;
	}
}

void reply_301(kconnection_t *c)
{
	K_FORCE_USE(c);
}

void reply_302(kconnection_t *c)
{
	K_FORCE_USE(c);
}
