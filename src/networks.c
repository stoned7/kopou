#include "kopou.h"

static void set_new_kclient(int fd, char *addr)
{
	K_FORCE_USE(fd);
	K_FORCE_USE(addr);

	kclient_t *c = xmalloc(sizeof(kclient_t));
	c->fd = fd;
	c->remoteaddr = kstr_new(addr);
	c->created_ts = time(NULL);
	c->last_access_ts = c->created_ts;
	c->req_type = KOPOU_REQ_TYPE_NONE;
	c->req_ready_to_process = 0;
	c->blueprint = NULL;
	c->disconnect_asap = 0;
	c->disconnect_after_write = 0;

	//kopou.clients[fd] = c;
	kopou.nclients++;
}


static void kclient_reader(int fd, eventtype_t evtype)
{
	K_FORCE_USE(fd);
	K_FORCE_USE(evtype);
}

static void kclient_writer(int fd, eventtype_t evtype)
{
	K_FORCE_USE(fd);
	K_FORCE_USE(evtype);
}

static void kclient_error(int fd, eventtype_t evtype)
{
	K_FORCE_USE(evtype);
	klog(KOPOU_ERR, "client err: %s", strerror(errno));
	/* free client */
	kclient_t *c = NULL; // = kopou.clients[fd];
	xfree(c);
	//kopou.clients[fd] = NULL;
	kopou.nclients--;
	tcp_close(fd);
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
