#include <stdlib.h>
#include "kopou.h"
#include "kevent.h"

kevent_loop_t *kevent_new(int size, onloop_prepoll onprepoll_handler,
				onloop_error onerror_handler)
{
	kevent_loop_t *el;
	el = xmalloc(sizeof(*el));

	el->epd = epoll_create(KEVENT_HINT);
	if (el->epd == KEVENT_ERR) {
		xfree(el);
		return NULL;
	}
	el->events = xcalloc(size, sizeof(struct epoll_event));
	el->kevents = xcalloc(size, sizeof(kevent_t));
	el->event_size = size;
	el->_event_size = 0;
	el->evcounter = 0;
	el->prepoll_handler = onprepoll_handler;
	el->error_handler = onerror_handler;
	el->stop = 0;

	return el;
}

void kevent_del(struct kevent_loop *el)
{
	xfree(el->events);
	xfree(el->kevents);
	xfree(el);
}

static void _reset_event(kevent_loop_t *el, kevent_t *ev)
{
	el->_event_size--;
	ev->eventtype = KEVENT_FREE;
	ev->fd = -1;
	ev->onerror_handler = NULL;
	ev->onread_handler = NULL;
	ev->onwrite_handler = NULL;
}

static void _process(kevent_loop_t *el, kevent_t *ev)
{
	if (ev->eventtype & KEVENT_FREE)
		return;

	if (ev->eventtype & KEVENT_FAULTY) {
		if (ev->onerror_handler != NULL)
			ev->onerror_handler(ev->fd, ev->eventtype);
		int fdtemp = ev->fd;
		eventtype_t evttemp = ev->eventtype;
		_reset_event(el, ev);
		klog(KOPOU_DEBUG, "fd: %d, del ev: %d, curr ev: %d, size: %d",
			fdtemp, evttemp, ev->eventtype, el->_event_size);
		return;
	}

	if (ev->eventtype & KEVENT_READABLE && ev->onread_handler != NULL)
			ev->onread_handler(ev->fd, ev->eventtype);
	if (ev->eventtype & KEVENT_WRITABLE && ev->onwrite_handler != NULL)
			ev->onwrite_handler(ev->fd, ev->eventtype);
}

static int _poll(kevent_loop_t *el)
{
	int nev, i;
	nev = epoll_wait(el->epd, el->events, el->event_size,
						KEVENT_POLL_TIMEOUT);
	if (nev == KEVENT_ERR) {
		el->error_handler(el, errno);
		return KEVENT_ERR;
	}

	for (i = 0; i < nev; i++) {
		kevent_t *ev;
		eventtype_t et = KEVENT_FREE;
		ev = &el->kevents[el->events[i].data.fd];

		if (el->events[i].events & EPOLLERR
				|| el->events[i].events & EPOLLHUP)
			et |= KEVENT_FAULTY;
		if (el->events[i].events & EPOLLIN)
			et |= KEVENT_READABLE;
		if (el->events[i].events & EPOLLOUT)
			et |= KEVENT_WRITABLE;

		ev->eventtype = et;
		_process(el, ev);
	}
	return nev;
}

int kevent_loop_start(kevent_loop_t *el)
{
	int r = KEVENT_OK;
	while (!el->stop) {
		if (el->prepoll_handler)
			el->prepoll_handler(el);
		if (el->stop)
			break;
		r = _poll(el);
		el->evcounter++;
	}
	return r;
}

/*
void kevent_loop_stop(kevent_loop_t *el)
{
	el->stop = 1;
}

int kevent_is_free(kevent_loop_t *el, int fd)
{
	if (fd >= el->event_size)
		return KEVENT_ERR;
	if (el->kevents[fd].eventtype == KEVENT_FREE)
		return 1;
	return 0;
}

int kevent_already_writable(kevent_loop_t *el, int fd)
{
	if (fd >= el->event_size)
		return KEVENT_ERR;
	if (el->kevents[fd].eventtype & KEVENT_WRITABLE)
		return 1;
	return 0;
}

int kevent_already_readable(kevent_loop_t *el, int fd)
{
	if (fd >= el->event_size)
		return KEVENT_ERR;
	if (el->kevents[fd].eventtype & KEVENT_READABLE)
		return 1;
	return 0;
}
*/

int kevent_add_event(kevent_loop_t *el, int fd, eventtype_t eventtype,
		onread onread_handler, onwrite onwrite_handler,
		onerror onerror_handler)
{
	int operation, r;
	struct epoll_event ev;
	kevent_t *kev;
	eventtype_t req_eventtype;

	if (fd >= el->event_size)
		return KEVENT_ERR;

	req_eventtype = eventtype;
	kev = &el->kevents[fd];
	if (kev->eventtype == KEVENT_FREE) {
		operation = EPOLL_CTL_ADD;
		el->_event_size++;
		kev->onerror_handler = onerror_handler;
	}
	else
		operation = EPOLL_CTL_MOD;

	ev.events = KEVENT_FREE;
	eventtype |= kev->eventtype;

	if (eventtype & KEVENT_READABLE)
		ev.events |= EPOLLIN;
	if (eventtype & KEVENT_WRITABLE)
		ev.events |= EPOLLOUT;

	ev.data.fd = fd;
	r = epoll_ctl(el->epd, operation, fd, &ev);
	if (r == KEVENT_ERR)
		return KEVENT_ERR;

	if (req_eventtype & KEVENT_READABLE)
		kev->onread_handler = onread_handler;
	if (req_eventtype & KEVENT_WRITABLE)
		kev->onwrite_handler = onwrite_handler;

	kev->eventtype = eventtype;
	kev->fd = fd;
	klog(KOPOU_DEBUG, "fd: %d, add ev: %d, curr ev: %d, size: %d",
		fd, req_eventtype, eventtype, el->_event_size);
	return KEVENT_OK;
}


void kevent_del_event(kevent_loop_t *el, int fd, eventtype_t eventtype)
{
	struct epoll_event ev;
	kevent_t *kev;

	if (fd >= el->event_size)
		return;

	kev = &el->kevents[fd];
	if (kev->eventtype == KEVENT_FREE)
		return;

	kev->eventtype &= ~(eventtype);
	ev.data.fd = fd;
	ev.events = KEVENT_FREE;
	if (kev->eventtype & KEVENT_READABLE)
		ev.events |= EPOLLIN;
	if (kev->eventtype & KEVENT_WRITABLE)
		ev.events |= EPOLLOUT;

	if (kev->eventtype == KEVENT_FREE) {
		epoll_ctl(el->epd, EPOLL_CTL_DEL, fd, &ev);
		_reset_event(el, kev);
	} else {
		epoll_ctl(el->epd, EPOLL_CTL_MOD, fd, &ev);
		if (!(kev->eventtype & KEVENT_READABLE))
			kev->onread_handler = NULL;
		if (!(kev->eventtype & KEVENT_WRITABLE))
			kev->onwrite_handler = NULL;
	}
	klog(KOPOU_DEBUG, "fd: %d, del ev: %d, curr ev: %d, size: %d",
				fd, eventtype, kev->eventtype, el->_event_size);
}

