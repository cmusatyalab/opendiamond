/*
 * miniRPC - TCP RPC library with asynchronous operations
 *
 * Copyright (C) 2007-2008 Carnegie Mellon University
 *
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU Lesser General Public License version 2.1, as shown in
 * the file COPYING.
 */

#ifndef MINIRPC_POLLSET
#error This header is for internal use by the miniRPC pollset implementation
#endif

#ifndef MINIRPC_POLLSET_IMPL_H
#define MINIRPC_POLLSET_IMPL_H

#define NOT_RUNNING -1

struct pollset {
	pthread_mutex_t lock;
	GHashTable *members;
	GQueue *dead;
	GTree *timers;
	GQueue *expired;
	int64_t serial;
	int64_t running_serial;
	pthread_t running_thread;
	pthread_cond_t serial_cond;

	const struct pollset_ops *ops;
	struct impl_data *impl;
	struct selfpipe *wakeup;

	pthread_mutex_t poll_lock;
};

struct poll_fd {
	int fd;
	poll_flags_t flags;
	struct timeval expires;  /* wall-clock time to fire timeout_fn */
	void *private;
	int dead;
	poll_callback_fn *readable_fn;
	poll_callback_fn *writable_fn;
	poll_callback_fn *hangup_fn;
	poll_callback_fn *error_fn;
	poll_callback_fn *timeout_fn;
};

struct pollset_ops {
	int (*create)(struct pollset *pset);
	void (*destroy)(struct pollset *pset);
	int (*add)(struct pollset *pset, struct poll_fd *pfd);
	int (*modify)(struct pollset *pset, struct poll_fd *pfd);
	void (*remove)(struct pollset *pset, struct poll_fd *pfd);
	int (*poll)(struct pollset *pset, int timeout_ms);
};

#endif
