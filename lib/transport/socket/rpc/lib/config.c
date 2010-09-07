/*
 * miniRPC - TCP RPC library with asynchronous operations
 *
 * Copyright (C) 2007-2008 Carnegie Mellon University
 *
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU Lesser General Public License version 2.1, as shown in
 * the file COPYING.
 */

#include <errno.h>
#include <pthread.h>
#define MINIRPC_INTERNAL
#include "internal.h"

#define set_config(set, var, val) do {				\
		pthread_mutex_lock(&(conn)->config_lock);	\
		(set)->config.var=(val);			\
		pthread_mutex_unlock(&(conn)->config_lock);	\
	} while (0)

exported int mrpc_set_disconnect_func(struct mrpc_connection *conn,
			mrpc_disconnect_fn *func)
{
	if (conn == NULL)
		return EINVAL;
	set_config(conn, disconnect, func);
	return 0;
}
