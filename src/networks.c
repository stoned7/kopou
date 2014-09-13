#include "kopou.h"

static void set_new_kclient(int fd, char *addr, int type)
{
	kclient_t *c = xmalloc(sizeof(kclient_t));
	c->fd = fd;
	c->remoteaddr = kstr_new(addr);
	c->created_ts = time(NULL);
	c->last_access_ts = c->created_ts;

	c->type = type;
	c->disconnect_after_write = 0;

	c->blueprint = NULL;
	c->req_type = KOPOU_REQ_TYPE_NONE;
	c->req_ready_to_process = 0;
	c->req_parsing_pos = 0;
	c->reqbuf_len = 0;
	c->reqbuf_read_len = 0;
	c->reqbuf = NULL;

	c->resbuf_len = 0;
	c->resbuf_written_pos = 0;
	c->resbuf = NULL;

	kopou.clients[fd] = c;
}

static void free_client(kclient_t *c)
{
	int fd = c->fd;
	kstr_del(c->remoteaddr);
	xfree(c->reqbuf);
	xfree(c);
	kopou.clients[fd] = NULL;

	eventtype_t evt = KEVENT_READABLE | KEVENT_WRITABLE;
	kevent_del_event(kopou.loop, fd, evt);
	tcp_close(fd);
}

static void reset_client(kclient_t *c)
{
	xfree(c->blueprint);
	c->blueprint = NULL;
	c->req_type = KOPOU_REQ_TYPE_NONE;
	c->req_ready_to_process = 0;
	c->req_parsing_pos = 0;
	c->reqbuf_len = 0;
	c->reqbuf_read_len = 0;

	xfree(c->reqbuf);
	c->reqbuf = NULL;

	c->resbuf_len = 0;
	c->resbuf_written_pos = 0;
	c->resbuf = NULL;
}

static void kclient_writer(int fd, eventtype_t evtype)
{
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
}

static void kclient_reader(int fd, eventtype_t evtype)
{
	ssize_t nread;
	int tryagain;

	kclient_t *c = kopou.clients[fd];

	if (c->reqbuf)
		c->reqbuf = xrealloc(c->reqbuf, c->reqbuf_len + REQ_BUFFER_SIZE);
	else
		c->reqbuf = xmalloc(REQ_BUFFER_SIZE);

	c->reqbuf_len += REQ_BUFFER_SIZE;
	nread = tcp_read(fd, c->reqbuf + c->reqbuf_read_len,
			c->reqbuf_len - c->reqbuf_read_len, &tryagain);
	if (nread > 0) {
		c->reqbuf_read_len += nread;
		klog(KOPOU_DEBUG, "Read: %d:%zd:%d",fd, nread, tryagain);
	} else {
		if (!tryagain) {
			klog(KOPOU_WARNING, "client disconnected: %d, %s",
						fd, strerror(errno));
			free_client(c);
			kopou.nclients--;
			return;
		}
	}

	if (!(evtype & KEVENT_WRITABLE)) {
		if (kevent_add_event(kopou.loop, fd, KEVENT_WRITABLE,
			NULL, kclient_writer, NULL) == KEVENT_ERR) {
			klog(KOPOU_WARNING, "reply registration fail: %d, %s"
					,fd, strerror(errno));
			free_client(c);
			kopou.nclients--;
			return;
		}
	}

	if (parse_req(c) == PARSE_ERR) {
		c->disconnect_after_write = 1;
	} else if (c->req_ready_to_process) {
		/* TODO call register cmd */
	}
}


static void kclient_error(int fd, eventtype_t evtype)
{
	K_FORCE_USE(evtype);
	klog(KOPOU_ERR, "client err:%d, %s",fd, strerror(errno));
	free_client(kopou.clients[fd]);
	kopou.nclients--;
}

void kopou_accept_new_connection(int fd, eventtype_t evtype)
{
	K_FORCE_USE(evtype);

	char remote_addr[ADDRESS_LENGTH];
	int max, conn;

	int tryagain;
	max = KOPOU_MAX_ACCEPT_CONN;
	while (max--) {
		conn = tcp_accept(fd, remote_addr, sizeof(remote_addr), NULL,
				KOPOU_TCP_NONBLOCK, &tryagain);
		if (conn == TCP_ERR) {
			if (tryagain)
				break;
			klog(KOPOU_ERR, "listener connection err: %d:%s",
					tryagain, strerror(errno));
			break;
		}

		if (kopou.nclients > settings.max_ccur_clients) {
			char msg[] = "-1\r\n#16$MaxClientExceeds\r\n";
			tcp_write(conn, msg, sizeof(msg), &tryagain);
			klog(KOPOU_WARNING, "exceed max concurrent connection limits");
			tcp_close(conn);
			continue;
		}

		if (settings.client_keepalive) {
			if (tcp_set_keepalive(conn, KOPOU_TCP_KEEPALIVE) ==
					TCP_ERR) {
				klog(KOPOU_WARNING, "tcp keepalive setting err: %s",
						strerror(errno));
			}
		}

		klog(KOPOU_DEBUG, "new conn(%d): %s", conn, remote_addr);
		if (kevent_add_event(kopou.loop, conn, KEVENT_READABLE,
			   kclient_reader, NULL, kclient_error) == KEVENT_ERR) {
			klog(KOPOU_WARNING, "new conn register fail, closing '%d'", conn);
			tcp_close(conn);
			continue;
		}
		set_new_kclient(conn, remote_addr, KOPOU_CLIENT_TYPE_NORMAL);
		kopou.nclients++;
	}
}

void kopou_listener_error(int fd, eventtype_t evtype)
{
	K_FORCE_USE(fd);
	K_FORCE_USE(evtype);
	klog(KOPOU_ERR, "listener err: %s", strerror(errno));
	kopou.shutdown = 1;
}
