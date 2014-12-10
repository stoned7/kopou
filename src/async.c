#include "async.h"
#include "xalloc.h"

static void *run_worker(void *arg);
static asynctask_t *get_task(async_t *a);


static asynctask_t *asynctask_new(void (*task)(void *arg), void *arg,
					void (*del)(void *arg))
{
	asynctask_t *t;

	t = xmalloc(sizeof(*t));
	t->task = task;
	t->del = del;
	t->arg = arg;
	t->next = NULL;

	return t;
}

static void asynctask_del(asynctask_t *t)
{
	if (t->del)
		t->del(t->arg);
	xfree(t);
}

static asynctask_queue_t *asynctask_queue_new(void)
{
	asynctask_queue_t *q;
	asynctask_t *t;

	q = xmalloc(sizeof(*q));
	t = asynctask_new(NULL, NULL, NULL);
	q->head = q->tail = t;
	return q;
}

static void asynctask_queue_del(asynctask_queue_t *q)
{
	asynctask_t *t, *tmp;

	t = q->head;
	while (t) {
		tmp = t->next;
		asynctask_del(t);
		t = tmp;
	}

	xfree(q);
}

async_t *async_new(unsigned int nworkers)
{
	async_t *a;
	asyncworker_t *w, *tmpw;
	int r;

	if (nworkers < 1)
		return NULL;

	a = xmalloc(sizeof(*a));
	a->q = asynctask_queue_new();
	a->stop = 0;

	r = pthread_mutex_init(&a->tlock, NULL);
	if (r)
		goto err;
	r = pthread_cond_init(&a->tsignal, NULL);
	if (r) {
		pthread_mutex_destroy(&a->tlock);
		goto err;
	}

	w = NULL;
	while (nworkers--) {
		tmpw = xmalloc(sizeof(*w));
		tmpw->next = w;
		w = tmpw;
		r = pthread_create(&tmpw->tid, NULL, run_worker, a);
		if (r) {
			async_del(a);
			a = NULL;
			break;
		}
	}
	a->workers = w;

	return a;
err:
	asynctask_queue_del(a->q);
	xfree(a);
	return NULL;
}

void async_del(async_t *a)
{
	asyncworker_t *w, *tmpw;

	a->stop = 1;
	pthread_mutex_lock(&a->tlock);
	pthread_cond_broadcast(&a->tsignal);
	pthread_mutex_unlock(&a->tlock);

	w = a->workers;
	while (w) {
		tmpw = w->next;
		pthread_join(w->tid, NULL);
		xfree(w);
		w = tmpw;
	}

	pthread_mutex_destroy(&a->tlock);
	pthread_cond_destroy(&a->tsignal);

	asynctask_queue_del(a->q);
	xfree(a);
}

static void asynctask_queue_enqueue(asynctask_queue_t *q, asynctask_t *at)
{
	q->tail->next = at;
	q->tail = at;
}

static int asynctask_queue_dequeue(asynctask_queue_t *q, asynctask_t **at)
{
	asynctask_t *nh;

	*at = q->head->nex;
	nh = q->head->next;
	if (!nh)
		return ASYNC_ERR;

	q->head = nh;
	return ASYNC_OK;
}

static asynctask_t *get_task(async_t *a)
{
	asynctask_t *at;
	int r;

	at = NULL;

	pthread_mutex_lock(&a->tlock);
	while (asynctask_queue_dequeue(a->q, &at) == ASYNC_ERR && !a->stop)
		if (pthread_cond_wait(&a->tsignal, &a->tlock))
			break;

	pthread_mutex_unlock(&a->tlock);
	return at;
}

static void *run_worker(void *arg)
{
	async_t *a;
	asynctask_t *at;

	a = arg;
	while (1) {
		at = get_task(a);
		if (at) {
			at->task(at->arg);
			asynctask_del(at);
		}

		if (a->stop)
			break;
	}
	pthread_exit(NULL);
}

int async_add_task(async_t *a, void (*task)(void *arg), void *arg,
	void (*del)(void *arg))
{
	asynctask_t *at;

	at = asynctask_new(task, arg, del);
	asynctask_queue_enqueue(a->q, at);

	pthread_mutex_lock(&a->tlock);
	pthread_cond_signal(&a->tsignal);
	pthread_mutex_unlock(&a->tlock);

	return ASYNC_OK;
}

