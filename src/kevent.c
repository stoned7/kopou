#include <stdlib.h>
#include "kopou.h"
#include "kevent.h"

struct kevent_loop* kevent_new(int size, onprepoll onprepoll_handler)
{
	int i;
	struct kevent_loop *el;
	el = xmalloc(sizeof(*el));

	el->epd = epoll_create(KEVENT_HINT);
	if (el->epd == KEVENT_ERR) {
		xfree(el);
		return NULL;
	}
	el->events = xcalloc(size, sizeof(struct epoll_event));
	el->kevents = xcalloc(size, sizeof(struct kevent));
	el->event_size = size;
	el->_event_size = 0;
	el->evcounter = 0;
	el->onprepoll_handler = onprepoll_handler;
	el->stop_asap = 0;

	return el;
}

void kevent_del(struct kevent_loop *el)
{
	xfree(el->events);
	xfree(el->kevents);
	xfree(el);
}

static void _reset_event(struct kevent_loop *el, struct kevent *ev)
{
	el->_event_size--;
	ev->eventtype = KEVENT_FREE;
	ev->fd = -1;
	ev->privatedata = NULL;
	ev->onerror_handler = NULL;
	ev->onread_handler = NULL;
	ev->onwrite_handler = NULL;
}

static void _process(struct kevent_loop *el, struct kevent *ev)
{
	if (ev->eventtype & KEVENT_FREE)
		return;

	if (ev->eventtype & KEVENT_FAULTY) {
		if (ev->onerror_handler != NULL)
			ev->onerror_handler(ev->fd, ev->privatedata);
		int fdtemp = ev->fd;
		eventtype_t evttemp = ev->eventtype;
		_reset_event(el, ev);
		klog(KOPOU_DEBUG, "fd: %d, del ev: %d, curr ev: %d, size: %d",
			fdtemp, evttemp, ev->eventtype, el->_event_size);
		return;
	}

	if (ev->eventtype & KEVENT_READABLE && ev->onread_handler != NULL)
			ev->onread_handler(ev->fd, ev->privatedata);
	if (ev->eventtype & KEVENT_WRITABLE && ev->onwrite_handler != NULL)
			ev->onwrite_handler(ev->fd, ev->privatedata);
}

static int _poll(struct kevent_loop *el)
{
	int nev, i;
	nev = epoll_wait(el->epd, el->events, el->event_size,
						KEVENT_POLL_TIMEOUT);
	if (nev == KEVENT_ERR) {
		el->stop_asap = 1;
		return KEVENT_ERR;
	}

	for (i = 0; i < nev; i++) {
		struct kevent *ev;
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

int kevent_loop_start(struct kevent_loop *el)
{
	int r = KEVENT_OK;
	while (!el->stop_asap) {
		if (el->onprepoll_handler != NULL)
			el->onprepoll_handler(el);
		r = _poll(el);
		el->evcounter++;
	}
	return r;
}


void kevent_loop_stop(struct kevent_loop *el)
{
	el->stop_asap = 1;
}

int kevent_is_free(struct kevent_loop *el, int fd)
{
	if (fd >= el->event_size)
		return KEVENT_ERR;
	if (el->kevents[fd].eventtype == KEVENT_FREE)
		return 1;
	return 0;
}

int kevent_already_writable(struct kevent_loop *el, int fd)
{
	if (fd >= el->event_size)
		return KEVENT_ERR;
	if (el->kevents[fd].eventtype & KEVENT_WRITABLE)
		return 1;
	return 0;
}

int kevent_already_readable(struct kevent_loop *el, int fd)
{
	if (fd >= el->event_size)
		return KEVENT_ERR;
	if (el->kevents[fd].eventtype & KEVENT_READABLE)
		return 1;
	return 0;
}

int kevent_add_event(struct kevent_loop *el, int fd, eventtype_t eventtype,
		onread onread_handler, onwrite onwrite_handler,
		onerror onerror_handler, void *privatedata)
{
	int operation, r;
	struct epoll_event ev;
	struct kevent *kev;
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
	kev->privatedata = privatedata;
	klog(KOPOU_DEBUG, "fd: %d, add ev: %d, curr ev: %d, size: %d",
		fd, req_eventtype, eventtype, el->_event_size);
	return KEVENT_OK;
}


void kevent_del_event(struct kevent_loop *el, int fd, eventtype_t eventtype)
{
	struct epoll_event ev;
	struct kevent *kev;

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

