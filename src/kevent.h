#ifndef __KEVENT_H__
#define __KEVENT_H__

#include <sys/epoll.h>
#include "xalloc.h"

#define KEVENT_POLL_TIMEOUT -1
#define KEVENT_OK 0
#define KEVENT_ERR -1

#define KEVENT_HINT 1024

enum {
	KEVENT_FREE = (0x000),
	KEVENT_READABLE = (0x001 << 0),
	KEVENT_WRITABLE = (0x001 << 1),
	KEVENT_FAULTY = (0x001 << 2),
};
typedef unsigned int eventtype_t;

struct kevent_loop;
struct kevent;

typedef void (*onprepoll)(struct kevent_loop *el);
typedef void (*onread)(int fd, void *privatedata);
typedef void (*onwrite)(int fd, void *privatedata);
typedef void (*onerror)(int fd, void *privatedata);

struct kevent {
	int fd;
	eventtype_t eventtype;
	void *privatedata;
	onread onread_handler;
	onwrite onwrite_handler;
	onerror onerror_handler;
};

struct kevent_loop {
	int epd;
	int _event_size;
	int event_size;
	struct kevent *kevents;
	struct epoll_event *events;
	int stop_asap;
	onprepoll onprepoll_handler;
	unsigned long long evcounter; /* only to debug */
};

struct kevent_loop* kevent_new(int size, onprepoll onprepoll_handler);
void kevent_del(struct kevent_loop *el);

int kevent_loop_start(struct kevent_loop *el);

int kevent_add_event(struct kevent_loop *el, int fd, eventtype_t eventtype,
		onread onread_handler, onwrite onwrite_handler,
		onerror onerror_handler, void *privatedata);

void kevent_del_event(struct kevent_loop *el, int fd, eventtype_t eventtype);
void kevent_loop_stop(struct kevent_loop *el);

int kevent_is_free(struct kevent_loop *el, int fd);
int kevent_already_writable(struct kevent_loop *el, int fd);
int kevent_already_readable(struct kevent_loop *el, int fd);

#endif
