/*
 * miniRPC - TCP RPC library with asynchronous operations
 *
 * Copyright (C) 2007-2008 Carnegie Mellon University
 *
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU Lesser General Public License version 2.1, as shown in
 * the file COPYING.
 */

#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>
#include <semaphore.h>
#include "common.h"

static struct {
	pthread_mutex_t lock;
	int disc_normal;
	int disc_ioerr;
	int disc_user;
	int ioerrs;
	int running_dispatchers;
	pthread_cond_t dispatcher_cond;
} stats = {
	.lock = PTHREAD_MUTEX_INITIALIZER,
	.dispatcher_cond = PTHREAD_COND_INITIALIZER
};

struct dispatcher_data {
	struct mrpc_conn_set *set;
	sem_t ready;
};

void _message(const char *file, int line, const char *func, const char *fmt,
			...)
{
	va_list ap;

	va_start(ap, fmt);
	fprintf(stderr, "%s line %d: %s(): ", file, line, func);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);
}

static void *monitored_dispatcher(void *data)
{
	struct dispatcher_data *ddata=data;
	struct mrpc_conn_set *set=ddata->set;

	mrpc_dispatcher_add(set);
	sem_post(&ddata->ready);
	expect(mrpc_dispatch_loop(set), ENXIO);
	mrpc_dispatcher_remove(set);
	pthread_mutex_lock(&stats.lock);
	stats.running_dispatchers--;
	pthread_mutex_unlock(&stats.lock);
	pthread_cond_broadcast(&stats.dispatcher_cond);
	return NULL;
}

void start_monitored_dispatcher(struct mrpc_conn_set *set)
{
	struct dispatcher_data ddata;
	pthread_t thr;
	pthread_attr_t attr;

	ddata.set=set;
	sem_init(&ddata.ready, 0, 0);
	pthread_mutex_lock(&stats.lock);
	stats.running_dispatchers++;
	pthread_mutex_unlock(&stats.lock);
	expect(pthread_attr_init(&attr), 0);
	expect(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED), 0);
	expect(pthread_create(&thr, &attr, monitored_dispatcher, &ddata), 0);
	expect(pthread_attr_destroy(&attr), 0);
	sem_wait(&ddata.ready);
	sem_destroy(&ddata.ready);
}

struct mrpc_conn_set *spawn_server(char **listen_port,
			const struct mrpc_protocol *protocol,
			mrpc_accept_fn accept, void *set_data, int threads)
{
	struct mrpc_conn_set *set;
	char *port=NULL;
	int ret;
	int i;

	if (mrpc_conn_set_create(&set, protocol, set_data))
		die("Couldn't allocate connection set");
	if (mrpc_set_accept_func(set, accept))
		die("Couldn't set accept function");
	ret=mrpc_listen(set, AF_INET, "localhost", &port);
	if (ret)
		die("%s", strerror(ret));
	for (i=0; i<threads; i++)
		start_monitored_dispatcher(set);
	if (listen_port)
		*listen_port=port;
	return set;
}

void disconnect_fatal(void *conn_data, enum mrpc_disc_reason reason)
{
	die("Unexpected disconnect: reason %d", reason);
}

void disconnect_normal_no_unref(void *conn_data, enum mrpc_disc_reason reason)
{
	if (reason != MRPC_DISC_CLOSED)
		die("Unexpected disconnect: reason %d", reason);
	pthread_mutex_lock(&stats.lock);
	stats.disc_normal++;
	pthread_mutex_unlock(&stats.lock);
}

void disconnect_normal(void *conn_data, enum mrpc_disc_reason reason)
{
	disconnect_normal_no_unref(conn_data, reason);
	mrpc_conn_unref(conn_data);
}

void disconnect_ioerr(void *conn_data, enum mrpc_disc_reason reason)
{
	if (reason != MRPC_DISC_IOERR)
		die("Unexpected disconnect: reason %d", reason);
	pthread_mutex_lock(&stats.lock);
	stats.disc_ioerr++;
	pthread_mutex_unlock(&stats.lock);
	mrpc_conn_unref(conn_data);
}

void disconnect_user(void *conn_data, enum mrpc_disc_reason reason)
{
	if (reason != MRPC_DISC_USER)
		die("Unexpected disconnect: reason %d", reason);
	pthread_mutex_lock(&stats.lock);
	stats.disc_user++;
	pthread_mutex_unlock(&stats.lock);
}

void disconnect_user_unref(void *conn_data, enum mrpc_disc_reason reason)
{
	disconnect_user(conn_data, reason);
	mrpc_conn_unref(conn_data);
}

void handle_ioerr(void *conn_private, char *msg)
{
	pthread_mutex_lock(&stats.lock);
	stats.ioerrs++;
	pthread_mutex_unlock(&stats.lock);
}

void dispatcher_barrier(void)
{
	struct timespec timeout = {0};

	timeout.tv_sec=time(NULL) + FAILURE_TIMEOUT;
	pthread_mutex_lock(&stats.lock);
	while (stats.running_dispatchers)
		if (pthread_cond_timedwait(&stats.dispatcher_cond, &stats.lock,
					&timeout) == ETIMEDOUT)
			die("Timed out waiting for dispatchers to exit "
						"(remaining: %d)",
						stats.running_dispatchers);
	pthread_mutex_unlock(&stats.lock);
}

void expect_disconnects(int user, int normal, int ioerr)
{
	dispatcher_barrier();
	pthread_mutex_lock(&stats.lock);
	if (user != -1 && stats.disc_user != user)
		die("Expected %d user disconnects, got %d", user,
					stats.disc_user);
	if (normal != -1 && stats.disc_normal != normal)
		die("Expected %d normal disconnects, got %d", normal,
					stats.disc_normal);
	if (ioerr != -1 && stats.disc_ioerr != ioerr)
		die("Expected %d ioerr disconnects, got %d", ioerr,
					stats.disc_ioerr);
	pthread_mutex_unlock(&stats.lock);
}

void expect_ioerrs(int count)
{
	dispatcher_barrier();
	pthread_mutex_lock(&stats.lock);
	if (stats.ioerrs != count)
		die("Expected %d I/O errors, got %d", count, stats.ioerrs);
	pthread_mutex_unlock(&stats.lock);
}
