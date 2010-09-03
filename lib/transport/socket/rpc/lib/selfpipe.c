/*
 * miniRPC - TCP RPC library with asynchronous operations
 *
 * Copyright (C) 2007-2008 Carnegie Mellon University
 *
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU Lesser General Public License version 2.1, as shown in
 * the file COPYING.
 */

#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <assert.h>
#define MINIRPC_INTERNAL
#include "internal.h"

struct selfpipe {
	int pipe[2];
	int set;
	pthread_mutex_t lock;
	pthread_cond_t cond;
};

int selfpipe_create(struct selfpipe **new)
{
	struct selfpipe *sp;
	int i;
	int ret;

	*new=NULL;
	sp=g_slice_new0(struct selfpipe);
	pthread_mutex_init(&sp->lock, NULL);
	pthread_cond_init(&sp->cond, NULL);
	if (pipe(sp->pipe)) {
		ret=errno;
		goto bad_free;
	}
	ret=set_nonblock(sp->pipe[0]);
	if (ret)
		goto bad_close;
	for (i=0; i < 2; i++) {
		ret=set_cloexec(sp->pipe[i]);
		if (ret)
			goto bad_close;
	}
	*new=sp;
	return 0;

bad_close:
	close(sp->pipe[1]);
	close(sp->pipe[0]);
bad_free:
	g_slice_free(struct selfpipe, sp);
	return ret;
}

void selfpipe_destroy(struct selfpipe *sp)
{
	close(sp->pipe[1]);
	close(sp->pipe[0]);
	g_slice_free(struct selfpipe, sp);
}

void selfpipe_set(struct selfpipe *sp)
{
	pthread_mutex_lock(&sp->lock);
	if (!sp->set) {
		while (write(sp->pipe[1], "a", 1) < 1);
		sp->set=1;
		pthread_cond_broadcast(&sp->cond);
	}
	pthread_mutex_unlock(&sp->lock);
}

void selfpipe_clear(struct selfpipe *sp)
{
	char buf[2];

	pthread_mutex_lock(&sp->lock);
	if (sp->set) {
		if (read(sp->pipe[0], buf, sizeof(buf)) != 1)
			assert(0);
		sp->set=0;
	}
	pthread_mutex_unlock(&sp->lock);
}

int selfpipe_is_set(struct selfpipe *sp)
{
	int ret;

	pthread_mutex_lock(&sp->lock);
	ret=sp->set;
	pthread_mutex_unlock(&sp->lock);
	return ret;
}

int selfpipe_fd(struct selfpipe *sp)
{
	return sp->pipe[0];
}

void selfpipe_wait(struct selfpipe *sp)
{
	pthread_mutex_lock(&sp->lock);
	while (!sp->set)
		pthread_cond_wait(&sp->cond, &sp->lock);
	pthread_mutex_unlock(&sp->lock);
}
