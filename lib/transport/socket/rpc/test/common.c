/*
 * miniRPC - TCP RPC library with asynchronous operations
 *
 * Copyright (C) 2007-2010 Carnegie Mellon University
 *
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU Lesser General Public License version 2.1, as shown in
 * the file COPYING.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
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
	struct mrpc_connection *conn;
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
	struct mrpc_connection *conn=ddata->conn;

	mrpc_dispatcher_add(conn);
	sem_post(&ddata->ready);
	expect(mrpc_dispatch_loop(conn), ENXIO);
	mrpc_dispatcher_remove(conn);
	pthread_mutex_lock(&stats.lock);
	stats.running_dispatchers--;
	pthread_mutex_unlock(&stats.lock);
	pthread_cond_broadcast(&stats.dispatcher_cond);
	return NULL;
}

void start_monitored_dispatcher(struct mrpc_connection *conn)
{
	struct dispatcher_data ddata;
	pthread_t thr;
	pthread_attr_t attr;

	ddata.conn=conn;
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

static void set_blocking(int fd, int blocking)
{
	int flags;

	flags = fcntl(fd, F_GETFL);
	expect(flags != -1, 1);
	if (blocking)
		flags &= ~O_NONBLOCK;
	else
		flags |= O_NONBLOCK;
	expect(fcntl(fd, F_SETFL, flags), 0);
}

void get_conn_pair(int *a, int *b)
{
	struct sockaddr_in addr;
	socklen_t addrlen;
	int lfd;
	int sfd;
	int cfd;

	lfd=socket(PF_INET, SOCK_STREAM, 0);
	if (lfd == -1)
		die("Couldn't create socket");
	addr.sin_family=AF_INET;
	addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
	addr.sin_port=0;
	if (bind(lfd, (struct sockaddr *)&addr, sizeof(addr)))
		die("Couldn't bind socket");
	if (listen(lfd, 16))
		die("Couldn't listen on socket");
	addrlen=sizeof(addr);
	if (getsockname(lfd, (struct sockaddr *)&addr, &addrlen))
		die("Couldn't get socket name");
	cfd=socket(PF_INET, SOCK_STREAM, 0);
	if (cfd == -1)
		die("Couldn't create socket");
	set_blocking(cfd, 0);
	if (connect(cfd, (struct sockaddr *)&addr, sizeof(addr)) != -1 ||
				errno != EINPROGRESS)
		die("Couldn't connect socket");
	sfd=accept(lfd, NULL, NULL);
	if (sfd == -1)
		die("Couldn't accept incoming connection");
	close(lfd);
	set_blocking(cfd, 1);
	*a = cfd;
	*b = sfd;
}

void bind_conn_pair(struct mrpc_connection *a, struct mrpc_connection *b)
{
	int fda;
	int fdb;

	get_conn_pair(&fda, &fdb);
	expect(mrpc_bind_fd(a, fda), 0);
	expect(mrpc_bind_fd(b, fdb), 0);
}

void disconnect_fatal(void *conn_data, enum mrpc_disc_reason reason)
{
	die("Unexpected disconnect: reason %d", reason);
}

void disconnect_normal(void *conn_data, enum mrpc_disc_reason reason)
{
	if (reason != MRPC_DISC_CLOSED)
		die("Unexpected disconnect: reason %d", reason);
	pthread_mutex_lock(&stats.lock);
	stats.disc_normal++;
	pthread_mutex_unlock(&stats.lock);
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
