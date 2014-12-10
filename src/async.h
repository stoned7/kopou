#ifndef __ASYNC_H__
#define __ASYNC_H__

#include <stdlib.h>
#include <pthread.h>

#define ASYNC_OK 0
#define ASYNC_ERR -1

typedef struct asyncworker_s {
	pthread_t tid;
	struct asyncworker_s *next;
} asyncworker_t;

typedef struct asynctask_s {
	struct  asynctask_s *next;
        void (*task)(void *arg);
        void *arg;
        void (*del)(void *arg);
} asynctask_t;

typedef struct {
	asynctask_t *head;
	asynctask_t *tail;
} asynctask_queue_t;

typedef struct {
	asyncworker_t *workers;
	asynctask_queue_t *q;
        pthread_mutex_t tlock;
        pthread_cond_t tsignal;
        volatile int stop;
} async_t;

async_t *async_new(unsigned int nworkers);
void async_del(async_t *a);
int async_add_task(async_t *a, void (*task)(void *arg), void *arg,
					void (*del)(void *arg));
#endif
