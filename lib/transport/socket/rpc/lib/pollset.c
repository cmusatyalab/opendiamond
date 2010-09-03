/*
 * miniRPC - TCP RPC library with asynchronous operations
 *
 * Copyright (C) 2007-2008 Carnegie Mellon University
 *
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU Lesser General Public License version 2.1, as shown in
 * the file COPYING.
 */

#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <assert.h>
#include <errno.h>
#define MINIRPC_POLLSET
#define MINIRPC_INTERNAL
#include "internal.h"
#include "pollset_impl.h"

extern const struct pollset_ops *ops_poll;
#ifdef HAVE_EPOLL
extern const struct pollset_ops *ops_epoll;
#else
const struct pollset_ops *ops_epoll = NULL;
#endif

struct timer_traverse_data {
	struct pollset *pset;
	struct timeval curtime;
	struct timeval next_expire;
};

static void poll_fd_free(struct poll_fd *pfd)
{
	g_slice_free(struct poll_fd, pfd);
}

/* pset lock must be held */
static void pollset_free_dead(struct pollset *pset)
{
	struct poll_fd *pfd;

	while ((pfd=g_queue_pop_head(pset->dead)) != NULL)
		poll_fd_free(pfd);
}

static int timeval_compare(const void *ta, const void *tb)
{
	const struct timeval *a=ta;
	const struct timeval *b=tb;

	/* Note: we never claim that two timevals are equal, even when they
	   are, unless their *pointers* are equal.  This is because we don't
	   want to clobber another poll_timer in the search tree that happens
	   to have the same expire time, but we *do* want to be able to remove
	   a timer from the tree if we have a pointer to it. */
	if (timercmp(a, b, >))
		return 1;
	else if (a != b)
		return -1;
	else
		return 0;
}

int pollset_alloc(struct pollset **new)
{
	struct pollset *pset;
	int ret;

	*new=NULL;
	pset=g_slice_new0(struct pollset);
	ret=selfpipe_create(&pset->wakeup);
	if (ret) {
		g_slice_free(struct pollset, pset);
		return ret;
	}
	pthread_mutex_init(&pset->lock, NULL);
	pthread_mutex_init(&pset->poll_lock, NULL);
	pthread_cond_init(&pset->serial_cond, NULL);
	pset->running_serial=NOT_RUNNING;
	pset->members=g_hash_table_new_full(g_int_hash, g_int_equal, NULL,
				(GDestroyNotify)poll_fd_free);
	pset->dead=g_queue_new();
	pset->timers=g_tree_new(timeval_compare);
	pset->expired=g_queue_new();
	pset->ops = ops_epoll ?: ops_poll;
again:
	ret=pset->ops->create(pset);
	if (ret == ENOSYS && pset->ops == ops_epoll) {
		pset->ops=ops_poll;
		goto again;
	} else if (ret) {
		goto cleanup;
	}
	ret=pollset_add(pset, selfpipe_fd(pset->wakeup), POLLSET_READABLE,
				NULL, NULL, NULL, NULL, assert_callback_func,
				NULL);
	if (ret) {
		pset->ops->destroy(pset);
		goto cleanup;
	}
	*new=pset;
	return 0;

cleanup:
	g_hash_table_destroy(pset->members);
	g_queue_free(pset->dead);
	selfpipe_destroy(pset->wakeup);
	g_slice_free(struct pollset, pset);
	return ret;
}

void pollset_free(struct pollset *pset)
{
	pthread_mutex_lock(&pset->lock);
	assert(pset->running_serial == NOT_RUNNING);
	pset->ops->destroy(pset);
	g_hash_table_destroy(pset->members);
	pollset_free_dead(pset);
	g_queue_free(pset->dead);
	g_tree_destroy(pset->timers);
	g_queue_free(pset->expired);
	selfpipe_destroy(pset->wakeup);
	pthread_mutex_unlock(&pset->lock);
	g_slice_free(struct pollset, pset);
}

int pollset_add(struct pollset *pset, int fd, poll_flags_t flags,
			void *private, poll_callback_fn *readable,
			poll_callback_fn *writable, poll_callback_fn *hangup,
			poll_callback_fn *error, poll_callback_fn *timeout)
{
	struct poll_fd *pfd;
	int ret;

	pfd=g_slice_new0(struct poll_fd);
	pfd->fd=fd;
	pfd->flags=flags;
	pfd->private=private;
	pfd->readable_fn=readable;
	pfd->writable_fn=writable;
	pfd->hangup_fn=hangup;
	pfd->error_fn=error;
	pfd->timeout_fn=timeout;
	pthread_mutex_lock(&pset->lock);
	if (g_hash_table_lookup(pset->members, &pfd->fd) != NULL) {
		pthread_mutex_unlock(&pset->lock);
		poll_fd_free(pfd);
		return EEXIST;
	}
	g_hash_table_replace(pset->members, &pfd->fd, pfd);
	ret=pset->ops->add(pset, pfd);
	if (ret)
		g_hash_table_remove(pset->members, &pfd->fd);
	pthread_mutex_unlock(&pset->lock);
	return ret;
}

int pollset_modify(struct pollset *pset, int fd, poll_flags_t flags)
{
	struct poll_fd *pfd;
	poll_flags_t old;
	int ret=0;

	pthread_mutex_lock(&pset->lock);
	pfd=g_hash_table_lookup(pset->members, &fd);
	if (pfd != NULL && flags != pfd->flags) {
		old=pfd->flags;
		pfd->flags=flags;
		ret=pset->ops->modify(pset, pfd);
		if (ret)
			pfd->flags=old;
	}
	pthread_mutex_unlock(&pset->lock);
	if (pfd == NULL)
		return EBADF;
	else
		return ret;
}

void pollset_del(struct pollset *pset, int fd)
{
	struct poll_fd *pfd;
	int64_t need_serial;

	pthread_mutex_lock(&pset->lock);
	pfd=g_hash_table_lookup(pset->members, &fd);
	if (pfd == NULL) {
		pthread_mutex_unlock(&pset->lock);
		return;
	}
	g_hash_table_steal(pset->members, &fd);
	g_tree_remove(pset->timers, &pfd->expires);
	need_serial=++pset->serial;
	pthread_mutex_unlock(&pset->lock);
	pset->ops->remove(pset, pfd);
	pollset_wake(pset);
	pthread_mutex_lock(&pset->lock);
	pfd->dead=1;
	g_queue_push_tail(pset->dead, pfd);
	while (pset->running_serial != NOT_RUNNING &&
				!pthread_equal(pset->running_thread,
				pthread_self()) &&
				pset->running_serial < need_serial)
		pthread_cond_wait(&pset->serial_cond, &pset->lock);
	pthread_mutex_unlock(&pset->lock);
}

int pollset_set_timer(struct pollset *pset, int fd, unsigned timeout_ms)
{
	struct poll_fd *pfd;
	struct timeval curtime;
	struct timeval timeout;
	struct timeval expire;

	gettimeofday(&curtime, NULL);
	timeout.tv_sec = timeout_ms / 1000;
	timeout.tv_usec = (timeout_ms % 1000) * 1000;
	timeradd(&curtime, &timeout, &expire);
	pthread_mutex_lock(&pset->lock);
	pfd=g_hash_table_lookup(pset->members, &fd);
	if (pfd == NULL) {
		pthread_mutex_unlock(&pset->lock);
		return EBADF;
	}
	if (timerisset(&pfd->expires)) {
		/* We don't implement resetting of pending timers, since the
		   straightforward implementation is racy and no one needs the
		   functionality right now */
		pthread_mutex_unlock(&pset->lock);
		return EBUSY;
	}
	pfd->expires=expire;
	g_tree_replace(pset->timers, &pfd->expires, pfd);
	pthread_mutex_unlock(&pset->lock);
	pollset_wake(pset);
	return 0;
}

static gboolean timer_traverse_func(void *key, void *value, void *data)
{
	struct timer_traverse_data *trav=data;
	struct timeval *tv=key;
	struct poll_fd *pfd=value;

	if (timercmp(tv, &trav->curtime, >)) {
		trav->next_expire=*tv;
		return FALSE;
	} else {
		g_queue_push_tail(trav->pset->expired, pfd);
		return TRUE;
	}
}

/* poll lock (but not pollset lock) must be held */
static void pollset_run_timers(struct pollset *pset, int *timeout_ms)
{
	struct timer_traverse_data data = {0};
	struct poll_fd *pfd;
	struct timeval timeout;

	data.pset=pset;
	gettimeofday(&data.curtime, NULL);
	pthread_mutex_lock(&pset->lock);
	g_tree_foreach(pset->timers, timer_traverse_func, &data);
	while ((pfd=g_queue_pop_head(pset->expired)) != NULL) {
		g_tree_remove(pset->timers, &pfd->expires);
		timerclear(&pfd->expires);
		if (pfd->timeout_fn && !pfd->dead) {
			pthread_mutex_unlock(&pset->lock);
			pfd->timeout_fn(pfd->private);
			pthread_mutex_lock(&pset->lock);
		}
	}
	pthread_mutex_unlock(&pset->lock);
	if (timeout_ms) {
		if (timerisset(&data.next_expire)) {
			gettimeofday(&data.curtime, NULL);
			timersub(&data.next_expire, &data.curtime, &timeout);
			*timeout_ms = max(timeout.tv_sec * 1000 +
						timeout.tv_usec / 1000, 1);
		} else {
			*timeout_ms = -1;
		}
	}
}

int pollset_poll(struct pollset *pset)
{
	int timeout;
	int ret;

	pthread_mutex_lock(&pset->poll_lock);
	pthread_mutex_lock(&pset->lock);
	pset->running_serial=pset->serial;
	pset->running_thread=pthread_self();
	pthread_mutex_unlock(&pset->lock);
	do {
		pollset_run_timers(pset, &timeout);
	} while ((ret=pset->ops->poll(pset, timeout)) == EINTR);
	pollset_run_timers(pset, NULL);
	pthread_mutex_lock(&pset->lock);
	pollset_free_dead(pset);
	pset->running_serial=NOT_RUNNING;
	pthread_cond_broadcast(&pset->serial_cond);
	pthread_mutex_unlock(&pset->lock);
	selfpipe_clear(pset->wakeup);
	pthread_mutex_unlock(&pset->poll_lock);
	return ret;
}

void pollset_wake(struct pollset *pset)
{
	selfpipe_set(pset->wakeup);
}
