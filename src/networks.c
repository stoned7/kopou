#include "kopou.h"

static kconnection_t *http_create_connection(int fd)
{
	kconnection_t *c = xmalloc(sizeof(kconnection_t));

	c->fd = fd;
	c->connection_type = CONNECTION_TYPE_HTTP;
	c->connection_ts = kopou.current_time;
	c->last_interaction_ts = -1;
	c->disconnect_after_reply = 0;

	c->req = xmalloc(sizeof(khttp_request_t));
	khttp_request_t *r = c->req;

	r->cmd = NULL;
	r->buf = r->curbuf = NULL;

	r->request_start = NULL;
	r->request_end = NULL;
	r->method_end = NULL;
	r->schema_start = NULL;
	r->schema_end = NULL;
	r->host_start = NULL;
	r->host_end = NULL;
	r->port_end = NULL;
	r->uri_start = NULL;
	r->args_start = NULL;
	r->uri_end = NULL;
	r->uri_ext = NULL;
	r->http_start = NULL;

	r->header_start = NULL;
	r->header_name_start = NULL;
	r->header_name_end = NULL;
	r->header_value_start = NULL;
	r->header_value_end = NULL;
	r->header_end = NULL;

	r->body = NULL;

	r->splitted_uri = NULL;
	r->headers = NULL;
	r->nsplitted_uri = 0;

	r->content_length = 0;
	r->content_type = NULL;
	r->_parsing_state = parsing_reqline_start;
	r->body_end_index = 0;

	r->connection_keepalive_timeout = 0;
	r->http_version = 0;
	r->major_version = 0;
	r->minor_version = 0;
	r->method = HTTP_METHOD_NOTSUPPORTED;
	r->connection_keepalive = 0;
	r->connection_close = 0;
	r->transfer_encoding_chunked = 0;
	r->complex_uri = 0;
	r->quoted_uri = 0;
	r->plus_in_uri = 0;
	r->space_in_uri = 0;

	r->res = xmalloc(sizeof(khttp_response_t));
	r->res->headers = NULL;
	r->res->nheaders = 0;
	r->res->buf = NULL;
	r->res->curbuf = NULL;
	r->res->flag = 0;

	return c;
}

static void http_delete_connection(kconnection_t *c)
{
	int i;
	kbuffer_t *b, *tb;
	knamevalue_t *h, *th;

	eventtype_t evt = KEVENT_READABLE | KEVENT_WRITABLE;
	kevent_del_event(kopou.loop, c->fd, evt);
	tcp_close(c->fd);

	if (c->req) {
		khttp_request_t *r = c->req;

		b = r->buf;
		while (b) {
			tb = b->next;
			xfree(b);
			b = tb;
		}

		h = r->headers;
		while (h) {
			th = h->next;
			xfree(h);
			h = th;
		}

		for (i = 0; i < r->nsplitted_uri; i++)
			kstr_del(r->splitted_uri[i]);

		if (r->content_type) {
			kstr_del(r->content_type);
			r->content_type = NULL;
		}

		for (i = 0; i < r->res->nheaders; i++)
			kstr_del(r->res->headers[i]);

		xfree(r->res->buf);
		xfree(r->res);
		xfree(r);
	}

	kopou.conns[c->fd] = NULL;
	kopou.nconns--;
	xfree(c);
};

static void http_reset_request(kconnection_t *c)
{
	int i;
	kbuffer_t *b, *tb;
	knamevalue_t *h, *th;

	khttp_request_t *r = c->req;

	b = r->buf;
	while (b) {
		tb = b->next;
		xfree(b);
		b = tb;
	}
	r->buf = r->curbuf = NULL;

	h = r->headers;
	while (h) {
		th = h->next;
		xfree(h);
		h = th;
	}
	r->headers = NULL;

	for (i = 0; i < r->nsplitted_uri; i++)
		kstr_del(r->splitted_uri[i]);
	r->splitted_uri = NULL;
	r->nsplitted_uri = 0;

	r->cmd = NULL;
	r->request_start = NULL;
	r->request_end = NULL;
	r->method_end = NULL;
	r->schema_start = NULL;
	r->schema_end = NULL;
	r->host_start = NULL;
	r->host_end = NULL;
	r->port_end = NULL;
	r->uri_start = NULL;
	r->args_start = NULL;
	r->uri_end = NULL;
	r->uri_ext = NULL;
	r->http_start = NULL;

	r->header_start = NULL;
	r->header_name_start = NULL;
	r->header_name_end = NULL;
	r->header_value_start = NULL;
	r->header_value_end = NULL;
	r->header_end = NULL;

	r->content_length = 0;
	if (r->content_type) {
		kstr_del(r->content_type);
		r->content_type = NULL;
	}
	r->_parsing_state = parsing_reqline_start;
	r->body_end_index = 0;

	r->connection_keepalive_timeout = 0;
	r->http_version = 0;
	r->major_version = 0;
	r->minor_version = 0;
	r->method = HTTP_METHOD_NOTSUPPORTED;
	r->connection_keepalive = 0;
	r->connection_close = 1;
	r->transfer_encoding_chunked = 0;
	r->complex_uri = 0;
	r->quoted_uri = 0;
	r->plus_in_uri = 0;
	r->space_in_uri = 0;

	xfree(r->res->headers);
	r->res->headers = NULL;
	r->res->nheaders = 0;
	r->res->flag = 0;

	xfree(r->res->buf);
	r->res->buf = r->res->curbuf = NULL;
};

static void http_response_handler(int fd, eventtype_t evtype)
{
	K_FORCE_USE(evtype);

	kconnection_t *c = kopou.conns[fd];
	khttp_request_t *r = c->req;

	ssize_t nw;
	int tryagain;

	kbuffer_t *b = r->res->curbuf;
	unsigned char *rb = b->pos;
	size_t rs = (b->last - b->pos) + 1;

	nw = tcp_write(c->fd, rb, rs, &tryagain);
	if (nw == TCP_ERR) {
		if (!tryagain) {
			klog(KOPOU_ERR, "reply err: %d, %s", c->fd,
					strerror(errno));
			http_delete_connection(c);
			return;
		}
	}
	b->pos = b->pos + nw;

	klog(KOPOU_DEBUG, "write: %zd bytes", nw);

	if (c->disconnect_after_reply) {
		http_delete_connection(c);
		return;
	}

	kevent_del_event(kopou.loop, fd, KEVENT_WRITABLE);
	http_reset_request(c);

	/*
	K_FORCE_USE(evtype);
	kclient_t *c = kopou.clients[fd];

	ssize_t written, twritten = 0;
	int tryagain, clientdead = 0;

	while (c->resbuf_len > 0) {
		written = tcp_write(fd, c->resbuf + c->resbuf_written_pos,
			c->resbuf_len - c->resbuf_written_pos, &tryagain);
		if (written == TCP_ERR) {
			if (!tryagain) {
				klog(KOPOU_ERR, "write err: %d, %s",
						fd, strerror(errno));
				free_client(c);
				kopou.nclients--;
				return;
			}
		}

		klog(KOPOU_DEBUG, "write: %zd bytes", written);
		twritten += written;
		c->resbuf_written_pos += written;

		if (c->resbuf_len == c->resbuf_written_pos) {
			klog(KOPOU_DEBUG, "write complete: %zu bytes", c->resbuf_len);
			if (c->disconnect_after_write || !settings.client_keepalive) {
				free_client(c);
				kopou.nclients--;
				clientdead = 1;
			} else {
				kevent_del_event(kopou.loop, fd, KEVENT_WRITABLE);
				reset_client(c);
			}
			break;
		}
		if (twritten > RES_WRITTEN_SIZE_MAX)
			break;
	}

	if (!clientdead && twritten > 0)
		c->last_access_ts = kopou.current_time;
	*/
}


static int http_prepare_to_reply(kconnection_t *c, eventtype_t evtype)
{
	if (!(evtype & KEVENT_WRITABLE)) {
		if (kevent_add_event(kopou.loop, c->fd, KEVENT_WRITABLE,
			NULL, http_response_handler, NULL) == KEVENT_ERR) {
			klog(KOPOU_WARNING, "reply registration fail: %d, %s",
							c->fd, strerror(errno));
			return K_ERR;
		}
	}
	return K_OK;
}

int http_set_system_header(khttp_request_t *r, char *h, int hl, char *v, int vl)
{
	if (!strncasecmp(HTTP_PROXY_CONNECTION, h, hl)
			|| !strncasecmp(HTTP_CONNECTION, h, hl)) {
		if (!strncasecmp(HTTP_CONN_CLOSE, v, vl)) {
			r->connection_close = 1;
			return HTTP_OK;
		} else if (!strncasecmp(HTTP_CONN_KEEP_ALIVE, v, vl)) {
			r->connection_keepalive = 1;
			return HTTP_OK;
		}
	} else if (!strncasecmp(HTTP_CONTENT_LENGTH, h, hl)) {
		errno = 0;
		r->content_length = strtoul(v, NULL, 10);
		if (errno) return HTTP_ERR;
		return HTTP_OK;
	} else if (!strncasecmp(HTTP_CONTENT_TYPE, h, hl)) {
		r->content_type = _kstr_create(v, vl);
		return HTTP_OK;
	} else if (!strncasecmp(HTTP_CONN_KEEP_ALIVE, h, hl)) {
		int i = atoi(v);
		if (i < 0 || i > 65536) return HTTP_ERR;
		r->connection_keepalive_timeout = i;
		return HTTP_OK;
	} else if (!strncasecmp(HTTP_TRANSFER_ENCODING, h, hl)) {
		if (!strncasecmp(HTTP_TE_CHUNKED, v, vl)) {
			r->transfer_encoding_chunked = 1;
			return HTTP_OK;
		}
	}
	return HTTP_CONTINUE;
}



static void http_request_handler(int fd, eventtype_t evtype)
{
	kbuffer_t *b, *nb;
	kconnection_t *c;
	khttp_request_t *r;
	char *rb;
	ssize_t nr;
	size_t rllen, rs, f, t;
	int hl, vl;
	int tryagain, re;

	c = kopou.conns[fd];
	r = c->req;

	b = r->buf;
	if (b == NULL) {
		b = xmalloc(sizeof(kbuffer_t));
		b->start = xmalloc(HTTP_REQ_BUFFER_SIZE << 1);
		b->end = b->start + ((HTTP_REQ_BUFFER_SIZE << 1) - 1);
		b->pos = b->last = b->start;
		b->next = NULL;
		rs = HTTP_REQ_BUFFER_SIZE << 1;
		rb = b->start;
		r->buf = r->curbuf = b;
	} else {
		if (r->_parsing_state >= parsing_header_done) {
			b = r->curbuf;
			f = (b->end - b->last) + 1;
			t = (b->end - b->start) + 1;
			if ((t - f) < (1024 << 2)) {
				nb = xmalloc(sizeof(kbuffer_t));
				nb->start = xmalloc(HTTP_REQ_BUFFER_SIZE);
				nb->end = nb->start + HTTP_REQ_BUFFER_SIZE - 1;
				nb->pos = nb->last = nb->start;
				nb->next = NULL;
				b->next = nb;
				b = nb;
				rb = nb->start;
				rs = HTTP_REQ_BUFFER_SIZE;
			} else {
				rb = b->last + 1;
				rs = t - f;
			}
		} else {
			rb = b->last + 1;
			rs = (b->end - b->last) + 1;
		}
	}

	printf("first real size: %d\n", HTTP_REQ_BUFFER_SIZE << 1);
	printf("cal size: %zu\n", rs);

	nr = tcp_read(fd, rb, rs, &tryagain);

	if (nr > 0) {
		klog(KOPOU_DEBUG, "http request: %zd", nr);
		r->curbuf->last += nr - 1;
	} else if (!tryagain) {
		klog(KOPOU_WARNING, "http connection disconnected: %d, %s",
					fd, strerror(errno));
		http_delete_connection(c);
		return;
	}

	if (http_prepare_to_reply(c, evtype) == K_ERR) {
		http_delete_connection(c);
		return;
	}

	re = http_parse_request_line(c);
	if (re == HTTP_ERR) {
		c->disconnect_after_reply = 1;
		return;
	} else if (re == HTTP_CONTINUE) {
		return;
	}

	//todo: total header size checks

	if (r->_parsing_state == parsing_reqline_done) {

		rllen = (r->args_start == NULL)
				? ((r->uri_end) - (r->uri_start + 1))
				: ((r->args_start -1) - (r->uri_start + 1));
		kstr_t uri = _kstr_create(r->uri_start + 1, rllen);
		r->nsplitted_uri = kstr_tok(uri, "/", &r->splitted_uri);
		kstr_del(uri);

		do {
			re = http_parse_header_line(c);
			if (re == HTTP_ERR) {
				c->disconnect_after_reply = 1;
				return;
			} else if (re == HTTP_CONTINUE) {
				return;
			}

			if (r->_parsing_state == parsing_header_line_done) {
				hl = r->header_name_end - r->header_name_start;
				if (hl > HTTP_HEADER_MAXLEN) {
					// TODO error 413 response
				}
				vl = r->header_value_end - r->header_value_start;
				if (http_set_system_header(r, r->header_name_start,
						hl, r->header_value_start, vl) == HTTP_ERR) {
					// TODO error badrequest response
					c->disconnect_after_reply = 1;
					return;
				}
			}
		} while (r->_parsing_state != parsing_header_done);

		if (r->_parsing_state == parsing_header_done) {

			if (r->connection_keepalive)
				r->connection_close = 1;

			r->_parsing_state = parsing_body_start;
			if (r->transfer_encoding_chunked) {
				re = http_parse_chunked_body(c);
				if (re == HTTP_ERR) {
					c->disconnect_after_reply = 1;
					return;
				} else if (re == HTTP_CONTINUE)
					return;
			} else if (r->content_length > 0) {
				r->body = r->header_end + 2;
			}
			//need to check the limit of body

		}

		reply_200(c);
	}
}


static void http_error_handler(int fd, eventtype_t evtype)
{
	K_FORCE_USE(evtype);
	klog(KOPOU_ERR, "http err:%d, %s",fd, strerror(errno));
	http_delete_connection(kopou.conns[fd]);
}

void http_accept_new_connection(int fd, eventtype_t evtype)
{
	K_FORCE_USE(evtype);

	char remote_addr[ADDRESS_LENGTH];
	int max, conn;
	int tryagain;

	max = KOPOU_HTTP_ACCEPT_MAX_CONN;
	while (max--) {
		conn = tcp_accept(fd, remote_addr, sizeof(remote_addr),
					NULL, KOPOU_TCP_NONBLOCK, &tryagain);
		if (conn == TCP_ERR) {
			if (tryagain) break;
			klog(KOPOU_ERR, "http listener connection err: %d:%s",
					tryagain, strerror(errno));
			break;
		}

		if (kopou.nconns > settings.http_max_ccur_conns) {
			char msg[] = "HTTP/1.1 503 Service unavailable\r\nConnection: close\r\n";
			tcp_write(conn, msg, sizeof(msg), &tryagain);
			klog(KOPOU_WARNING, "exceed max concurrent connection limits: %zu",
								kopou.nconns);
			tcp_close(conn);
			continue;
		}

		if (settings.tcp_keepalive) {
			if (tcp_set_keepalive(conn,
				KOPOU_DEFAULT_TCP_KEEPALIVE_INTERVAL) == TCP_ERR) {
				klog(KOPOU_WARNING, "tcp keepalive setting err: %s",
						strerror(errno));
			}
		}

		klog(KOPOU_DEBUG, "new http conn:(%d)", conn);
		if (kevent_add_event(kopou.loop, conn, KEVENT_READABLE,
			   http_request_handler, NULL, http_error_handler) == KEVENT_ERR) {
			klog(KOPOU_WARNING, "new conn register fail, closing %d: %s", conn, strerror(errno));
			tcp_close(conn);
			continue;
		}
		kopou.conns[conn] = http_create_connection(conn);
		kopou.nconns++;
	}
}

void http_listener_error(int fd, eventtype_t evtype)
{
	K_FORCE_USE(fd);
	K_FORCE_USE(evtype);
	klog(KOPOU_ERR, "http listener err: %s", strerror(errno));
	kopou.shutdown = 1;
}
