#include "kopou.h"

static void set_new_kclient(int fd, char *addr)
{
	kclient_t *c = xmalloc(sizeof(kclient_t));
	c->fd = fd;
	c->remoteaddr = kstr_new(addr);
	c->created_ts = time(NULL);
	c->last_access_ts = c->created_ts;

	c->blueprint = NULL;
	c->disconnect_asap = 0;
	c->disconnect_after_write = 0;

	c->req_type = KOPOU_REQ_TYPE_NONE;
	c->req_ready_to_process = 0;
	c->req_parsing_pos = 0;
	c->reqbuf_len = 0;
	c->reqbuf_read_len = 0;
	c->reqbuf = NULL;

	c->resbuf_len = 0;
	c->resbuf_writen_pos = 0;
	c->resbuf = NULL;

	kopou.clients[fd] = c;
	kopou.nclients++;
}

static void client_buf_delete(void *buf)
{
	xfree(buf);
}

static void free_client(int fd)
{
	kclient_t *c = kopou.clients[fd];
	kstr_del(c->remoteaddr);
	list_del(c->reqbuf);
	xfree(c->resbuf);
	xfree(c);
	kopou.clients[fd] = NULL;
	kopou.nclients--;

	eventtype_t evt = KEVENT_READABLE | KEVENT_WRITABLE;
	kevent_del_event(kopou.loop, fd, evt);
	tcp_close(fd);
}

static void kclient_writer(int fd, eventtype_t evtype)
{
	K_FORCE_USE(fd);
	K_FORCE_USE(evtype);
}

static void kclient_reader(int fd, eventtype_t evtype)
{
	ssize_t nread;
	int tryagain;

	kclient_t *c = kopou.clients[fd];
	if (!c->reqbuf)
		c->reqbuf = list_new(client_buf_delete);

	char *buf = xcalloc(REQ_BUFFER_SIZE, sizeof(char));
	list_add_next(c->reqbuf, list_tail(c->reqbuf), buf);
	c->reqbuf_len += REQ_BUFFER_SIZE;
	c->last_access_ts = kopou.current_time;

	nread = tcp_read(fd, buf, REQ_BUFFER_SIZE, &tryagain);
	if (nread > 0) {
		klog(KOPOU_DEBUG, "Read: %d:%zd:%d",fd, nread, tryagain);
		if (!(evtype & KEVENT_WRITABLE)) {
			if (kevent_add_event(kopou.loop, fd, KEVENT_WRITABLE,
				NULL, kclient_writer, NULL) == KEVENT_ERR) {
				klog(KOPOU_WARNING, "client reply handler fail: %d, %s"
							,fd, strerror(errno));
				free_client(fd);
			}
		}
	} else if (!tryagain) {
		klog(KOPOU_WARNING, "client disconnected: %d, %s",
						fd, strerror(errno));
		free_client(fd);
	}
	//parse req and execute
	//reply, clear elements reqbuff
}

static void kclient_error(int fd, eventtype_t evtype)
{
	K_FORCE_USE(evtype);
	klog(KOPOU_ERR, "client err:%d, %s",fd, strerror(errno));
	free_client(fd);
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

		if (kopou.nclients >= (settings.max_ccur_clients + KOPOU_OWN_FDS)) {
			klog(KOPOU_WARNING, "kopou reached max concurrent connection limits");
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
		set_new_kclient(conn, remote_addr);
	}
}

void kopou_listener_error(int fd, eventtype_t evtype)
{
	K_FORCE_USE(fd);
	K_FORCE_USE(evtype);
	klog(KOPOU_ERR, "listener err: %s", strerror(errno));
	kopou.shutdown = 1;
}
