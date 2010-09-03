/*
 * miniRPC - TCP RPC library with asynchronous operations
 *
 * Copyright (C) 2007-2008 Carnegie Mellon University
 *
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU Lesser General Public License version 2.1, as shown in
 * the file COPYING.
 */

#include <pthread.h>
#include <assert.h>
#include <poll.h>
#include <errno.h>
#define MINIRPC_POLLSET
#define MINIRPC_INTERNAL
#include "internal.h"
#include "pollset_impl.h"

struct poll_table {
	struct poll_fd **pfd;
	struct pollfd *ev;
	int idx;
	int count;
};

static int poll_create_set(struct pollset *pset)
{
	return 0;
}

static void poll_destroy_set(struct pollset *pset)
{
	return;
}

static int poll_add(struct pollset *pset, struct poll_fd *pfd)
{
	pollset_wake(pset);
	return 0;
}

static int poll_modify(struct pollset *pset, struct poll_fd *pfd)
{
	pollset_wake(pset);
	return 0;
}

static void poll_remove(struct pollset *pset, struct poll_fd *pfd)
{
	return;
}

static void poll_build_table(gpointer key, gpointer value, gpointer data)
{
	struct poll_table *pt=data;
	struct poll_fd *pfd=value;
	struct pollfd *ev;

	assert(pt->idx < pt->count);
	pt->pfd[pt->idx]=pfd;
	ev=&pt->ev[pt->idx++];
	ev->fd=pfd->fd;
	if (pfd->flags & POLLSET_READABLE)
		ev->events |= POLLIN;
	if (pfd->flags & POLLSET_WRITABLE)
		ev->events |= POLLOUT;
	/* POLLERR and POLLHUP are implicitly set */
}

static int poll_poll(struct pollset *pset, int timeout)
{
	struct poll_table pt;
	struct poll_fd *pfd;
	struct pollfd *ev;
	int i;

	pthread_mutex_lock(&pset->lock);
	pt.idx=0;
	pt.count=g_hash_table_size(pset->members);
	pt.pfd=g_new0(struct poll_fd *, pt.count);
	pt.ev=g_new0(struct pollfd, pt.count);
	g_hash_table_foreach(pset->members, poll_build_table, &pt);
	pthread_mutex_unlock(&pset->lock);

	if (poll(pt.ev, pt.count, timeout) == -1) {
		g_free(pt.ev);
		g_free(pt.pfd);
		return errno;
	}

	for (i=0; i<pt.count; i++) {
		pfd=pt.pfd[i];
		ev=&pt.ev[i];
		if ((ev->revents & POLLOUT) && pfd->writable_fn &&
					(pfd->flags & POLLSET_WRITABLE) &&
					!pfd->dead)
			pfd->writable_fn(pfd->private);
		if ((ev->revents & POLLIN) && pfd->readable_fn &&
					(pfd->flags & POLLSET_READABLE) &&
					!pfd->dead)
			pfd->readable_fn(pfd->private);
		if ((ev->revents & (POLLERR|POLLHUP)) && !pfd->dead) {
			if ((ev->revents & POLLHUP) && pfd->hangup_fn)
				pfd->hangup_fn(pfd->private);
			else if (pfd->error_fn)
				pfd->error_fn(pfd->private);
			pollset_del(pset, pfd->fd);
		}
	}
	g_free(pt.ev);
	g_free(pt.pfd);
	return 0;
}

static const struct pollset_ops ops = {
	.create = poll_create_set,
	.destroy = poll_destroy_set,
	.add = poll_add,
	.modify = poll_modify,
	.remove = poll_remove,
	.poll = poll_poll
};
const struct pollset_ops *ops_poll = &ops;
