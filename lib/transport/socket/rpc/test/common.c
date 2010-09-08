/*
 * miniRPC - Simple TCP RPC library
 *
 * Copyright (C) 2007-2010 Carnegie Mellon University
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include "common.h"

static struct {
	pthread_mutex_t lock;
	int running_dispatchers;
	pthread_cond_t dispatcher_cond;
} stats = {
	.lock = PTHREAD_MUTEX_INITIALIZER,
	.dispatcher_cond = PTHREAD_COND_INITIALIZER
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
	mrpc_dispatch_loop(data);
	/* Connection has been closed; clean it up */
	mrpc_conn_free(data);
	pthread_mutex_lock(&stats.lock);
	stats.running_dispatchers--;
	pthread_mutex_unlock(&stats.lock);
	pthread_cond_broadcast(&stats.dispatcher_cond);
	return NULL;
}

void start_monitored_dispatcher(struct mrpc_connection *conn)
{
	pthread_t thr;
	pthread_attr_t attr;

	pthread_mutex_lock(&stats.lock);
	stats.running_dispatchers++;
	pthread_mutex_unlock(&stats.lock);
	expect(pthread_attr_init(&attr), 0);
	expect(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED), 0);
	expect(pthread_create(&thr, &attr, monitored_dispatcher, conn), 0);
	expect(pthread_attr_destroy(&attr), 0);
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
