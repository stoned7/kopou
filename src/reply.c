#include "kopou.h"

static void set_http_response_headers(kconnection_t *c, kbuffer_t *b)
{
	int i;
	size_t s;
	kstr_t h;
	char fh[128];

	khttp_request_t *r = c->req;

	for (i = 0; i < r->res->nheaders; i++) {
		h = r->res->headers[i];
		s = kstr_len(h);
		memcpy(b->last +1, h, s);
		b->last = b->last + s;
	}

	get_http_date(fh, 128);
	s = strlen(fh);
	memcpy(b->last +1, fh, s);
	b->last = b->last + s;

	get_http_server_str(fh, 128);
	s = strlen(fh);
	memcpy(b->last +1, fh, s);
	b->last = b->last + s;
}

static void reply_err(kconnection_t *c, char *rl, int rllen)
{
	khttp_request_t *r;
	kbuffer_t *b;
	char *h;
	size_t s;

	r = c->req;
	b = r->res->buf;
	if (!b) {
		b = create_kbuffer(r->res->size_hint);
		r->res->buf = r->res->curbuf = b;
	}
	b->pos = b->last = b->start;

	memcpy(b->start, rl, rllen);
	b->last = b->start + rllen -1;

	if (settings.http_close_connection_onerror
		|| !settings.http_keepalive
		|| r->connection_close
		|| !r->connection_keepalive) {
		h = HTTP_H_CONNECTION_CLOSE;
		c->disconnect_after_reply = 1;
	}
	else
		h = HTTP_H_CONNECTION_KEEPALIVE;

	s = strlen(h);
	memcpy(b->last +1, h, s);
	b->last = b->last + s;

	h = HTTP_H_NO_CACHE;
	s = strlen(h);
	memcpy(b->last +1, h, s);
	b->last = b->last + s;

	h = "Content-Length: 0\r\n";
	s = strlen(h);
	memcpy(b->last +1, h, s);
	b->last = b->last + s;

	set_http_response_headers(c, b);
}

void reply_400(kconnection_t *c)
{
	char rl[] = "HTTP/1.1 400 Bad Request\r\n";
	reply_err(c, rl, strlen(rl));
}

void reply_403(kconnection_t *c)
{
	char rl[] = "HTTP/1.1 403 Forbidden NoPutRequest\r\n";
	reply_err(c, rl, strlen(rl));
}

void reply_413(kconnection_t *c)
{
	char rl[] = "HTTP/1.1 413 Request TooLarge\r\n";
	reply_err(c, rl, strlen(rl));
}

void reply_404(kconnection_t *c)
{
	char rl[] = "HTTP/1.1 404 Not Found\r\n";
	reply_err(c, rl, strlen(rl));
}

void reply_405(kconnection_t *c)
{
	char rl[] = "HTTP/1.1 405 Method NotAllowed\r\n";
	reply_err(c, rl, strlen(rl));
}

void reply_411(kconnection_t *c)
{
	char rl[] = "HTTP/1.1 411 Length Required\r\n";
	reply_err(c, rl, strlen(rl));
}

void reply_500(kconnection_t *c)
{
	char rl[] = "HTTP/1.1 500 Server Internal Error\r\n";
	reply_err(c, rl, strlen(rl));
}

void reply_501(kconnection_t *c)
{
	char rl[] = "HTTP/1.1 501 Not Implemented\r\n";
	reply_err(c, rl, strlen(rl));
}

void reply_503_now(kbuffer_t *b)
{
	char fh[128];
	char *h;

	char rl[] = "HTTP/1.1 503 Server MaximumConnection\r\n";
	size_t s = strlen(rl);

	b->pos = b->last = b->start;

	memcpy(b->start, rl, s);
	b->last = b->start + s -1;

	h = HTTP_H_CONNECTION_CLOSE;
	s = strlen(h);
	memcpy(b->last +1, h, s);
	b->last = b->last + s;

	h = HTTP_H_NO_CACHE;
	s = strlen(h);
	memcpy(b->last +1, h, s);
	b->last = b->last + s;

	get_http_date(fh, 128);
	s = strlen(fh);
	memcpy(b->last +1, fh, s);
	b->last = b->last + s;

	get_http_server_str(fh, 128);
	s = strlen(fh);
	memcpy(b->last +1, fh, s);
	b->last = b->last + s;

	h = "Content-Length: 0\r\n\r\n";
	s = strlen(h);
	memcpy(b->last +1, h, s);
	b->last = b->last + s;
}

void reply_505(kconnection_t *c)
{
	char rl[] = "HTTP/1.1 505 VersionNotSupported\r\n";
	reply_err(c, rl, strlen(rl));
}

static void reply_success(kconnection_t *c, char *rl, int rllen)
{
	khttp_request_t *r;
	kbuffer_t *b;

	size_t s;
	char *h;

	r = c->req;
	b = r->res->buf;

	if (!b) {
		b = create_kbuffer(r->res->size_hint);
		r->res->buf = r->res->curbuf = b;
	}

	memcpy(b->start, rl, rllen);
	b->last = b->start + rllen -1;

	if (settings.http_keepalive && r->connection_keepalive)
		h = HTTP_H_CONNECTION_KEEPALIVE;
	else {
		h = HTTP_H_CONNECTION_CLOSE;
		c->disconnect_after_reply = 1;
	}
	s = strlen(h);
	memcpy(b->last +1, h, s);
	b->last = b->last + s;

	if (r->res->flag & HTTP_RES_CACHABLE)
		h = HTTP_H_YES_CACHE;
	else
		h = HTTP_H_NO_CACHE;
	s = strlen(h);
	memcpy(b->last +1, h, s);
	b->last = b->last + s;

	if (!(r->res->flag & HTTP_RES_LENGTH)) {
		h = "Content-Length: 0\r\n";
		s = strlen(h);
		memcpy(b->last +1, h, s);
		b->last = b->last + s;
	}

	set_http_response_headers(c, b);
}

void reply_200(kconnection_t *c)
{
	char *rl = "HTTP/1.1 200 OK\r\n";
	reply_success(c, rl, strlen(rl));
}

void reply_201(kconnection_t *c)
{
	char *rl = "HTTP/1.1 201 Created\r\n";
	reply_success(c, rl, strlen(rl));
}

void reply_301(kconnection_t *c)
{
	char *rl = "HTTP/1.1 301 Permanent Moved\r\n";
	reply_success(c, rl, strlen(rl));
}

void reply_302(kconnection_t *c)
{
	char *rl = "HTTP/1.1 302 Moved\r\n";
	reply_success(c, rl, strlen(rl));
}
