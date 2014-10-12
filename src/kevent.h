#ifndef __KEVENT_H__
#define __KEVENT_H__

#include <sys/epoll.h>
#include <errno.h>
#include "xalloc.h"

#define KEVENT_POLL_TIMEOUT -1
#define KEVENT_OK 0
#define KEVENT_ERR -1

#define KEVENT_HINT 1024
#define KKK 1
enum {
	KEVENT_FREE = (0x000),
	KEVENT_READABLE = (0x001 << 0),
	KEVENT_WRITABLE = (0x001 << 1),
	KEVENT_FAULTY = (0x001 << 2)
};

typedef unsigned int eventtype_t;

struct kevent_loop;

typedef void (*onloop_prepoll)(struct kevent_loop *el);
typedef void (*onloop_error)(struct kevent_loop *el, int eerrno);
typedef void (*onread)(int fd, eventtype_t evtype);
typedef void (*onwrite)(int fd, eventtype_t evtype);
typedef void (*onerror)(int fd, eventtype_t evtype);

typedef struct kevent {
	int fd;
	eventtype_t eventtype;
	onread onread_handler;
	onwrite onwrite_handler;
	onerror onerror_handler;
} kevent_t;

typedef struct kevent_loop {
	int epd;
	int _event_size;
	int event_size;
	kevent_t *kevents;
	struct epoll_event *events;
	int stop;
	onloop_prepoll prepoll_handler;
	onloop_error error_handler;
	unsigned long long evcounter; /* only to debug */
} kevent_loop_t;

kevent_loop_t *kevent_new(int size, onloop_prepoll onprepoll_handler,
		onloop_error onerror_handler);
void kevent_del(kevent_loop_t *el);

int kevent_loop_start(kevent_loop_t *el);

int kevent_add_event(kevent_loop_t *el, int fd, eventtype_t eventtype,
		onread onread_handler, onwrite onwrite_handler,
		onerror onerror_handler);

void kevent_del_event(kevent_loop_t *el, int fd, eventtype_t eventtype);


static inline void kevent_loop_stop(kevent_loop_t *el)
{
	el->stop = 1;
}

static inline int kevent_is_free(kevent_loop_t *el, int fd)
{
	if (fd >= el->event_size)
		return KEVENT_ERR;
	if (el->kevents[fd].eventtype == KEVENT_FREE)
		return 1;
	return 0;
}

static inline int kevent_already_writable(kevent_loop_t *el, int fd)
{
	if (fd >= el->event_size)
		return KEVENT_ERR;
	if (el->kevents[fd].eventtype & KEVENT_WRITABLE)
		return 1;
	return 0;
}

static inline int kevent_already_readable(kevent_loop_t *el, int fd)
{
	if (fd >= el->event_size)
		return KEVENT_ERR;
	if (el->kevents[fd].eventtype & KEVENT_READABLE)
		return 1;
	return 0;
}

#endif
