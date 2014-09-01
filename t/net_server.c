#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "../src/kevent.h"
#include "../src/xalloc.h"
#include "../src/tcp.h"
#include "../src/kopou.h"

#define RBUFFSIZE 100
#define WBUFFSIZE 4

struct kevent_loop *el;
int listener;

void reader(int fd, void *privatedata);
void writer(int fd, void *privatedata);
void faulter(int fd, void *privatedata);

void reader(int fd, void *privatedata)
{
	ssize_t nread;
	int tryagain;
	char buf[RBUFFSIZE];
	memset(buf, '\0', RBUFFSIZE);
	nread = tcp_read(fd, buf, RBUFFSIZE, &tryagain);
	if (nread > 0) {
		klog(KOPOU_DEBUG, "Read: %d:%zd:%d",fd, nread, tryagain);
		FILE *fp = fopen("kopou.db", "a");
		fprintf(fp, "%s", buf);
		fflush(fp);
		fclose(fp);
		if (nread != RBUFFSIZE) {
		if (!kevent_already_writable(el, fd))
			kevent_add_event(el, fd, KEVENT_WRITABLE, NULL, writer, NULL, NULL);
		}
	} else {
		if (tryagain)
			klog(KOPOU_DEBUG, "completed read: %zd, tryagain:%d",
				nread, tryagain);
		else {
			klog(KOPOU_DEBUG, "conn down: %zd, %s",
							nread, strerror(errno));
			eventtype_t evt = KEVENT_READABLE | KEVENT_WRITABLE;
			kevent_del_event(el, fd, evt);
			tcp_close(fd);
		}
	}
}

void writer(int fd, void *privatedata)
{
	int tryagain;
	char *buf = "+0\r\n";
	ssize_t written = tcp_write(fd,  buf, WBUFFSIZE, &tryagain);
	klog(KOPOU_DEBUG, "written: %d:%zd:%d", fd, written, tryagain);
	if (written == WBUFFSIZE) {
		kevent_del_event(el, fd, KEVENT_WRITABLE);
		klog(KOPOU_DEBUG, "writer %d closed", fd);
	}
	if (written == TCP_ERR) {
		if (!tryagain) {
			eventtype_t evt = KEVENT_READABLE | KEVENT_WRITABLE;
			kevent_del_event(el, fd, evt);
			tcp_close(fd);
			klog(KOPOU_DEBUG, "removed %d events, writter err", fd);
		}
	}

}

void faulter(int fd, void *privatedata)
{
	klog(KOPOU_DEBUG, "faulter: %d", fd);
	tcp_close (fd);
}


void oom_handler(size_t size)
{
	_kdie("oom: %lu", size);
}

void prepoll_handler(struct kevent_loop *ev)
{
	klog(KOPOU_DEBUG, "prepoll: %llu", ev->evcounter);
}

void listener_err_handler(int fd, void *privatedata)
{
	klog(KOPOU_ERR, "listener err: %d", fd);
	kevent_loop_stop(el);
}


void listener_accept_handler(int fd, void *privatedata)
{
	int max = 1000;
	while (max--) {
		char ip[64];
		int port;
		int tryagain;
		int con = tcp_accept(fd, ip, 64, &port, 1, &tryagain);
		if (con == TCP_ERR) {
			if (tryagain)
				break;
			klog(KOPOU_ERR, "accept():%d:%s" ,tryagain,
							strerror(errno));
			break;
		}
		//tcp_set_keepalive(con, 10);
		klog(KOPOU_DEBUG, "-> conn(%d): %s:%d",con, ip, port);
		int r = kevent_add_event(el, con, KEVENT_READABLE, reader,
					NULL, faulter, NULL);
		if (r == KEVENT_ERR) {
			klog(KOPOU_ERR, "Reader add fail for %d", con);
			tcp_close(con);
		}
	}
}


int main()
{
	xalloc_set_oom_handler(oom_handler);

	listener = tcp_create_listener("127.0.0.1", 7878, 1);
	if (listener == TCP_ERR) {
		klog(KOPOU_ERR, "listener creation fail");
		return EXIT_FAILURE;
	}

	el = kevent_new(100, prepoll_handler);
	int aresult = kevent_add_event(el, listener, KEVENT_READABLE,
				listener_accept_handler, NULL,
				listener_err_handler, NULL);
	klog(KOPOU_INFO, "[%d] server start listening ...", listener);

	kevent_loop_start(el);

	klog(KOPOU_DEBUG, "deleting loop");
	kevent_del(el);
	return EXIT_SUCCESS;
}
