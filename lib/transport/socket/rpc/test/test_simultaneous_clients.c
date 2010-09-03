/*
 * miniRPC - TCP RPC library with asynchronous operations
 *
 * Copyright (C) 2007-2008 Carnegie Mellon University
 *
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU Lesser General Public License version 2.1, as shown in
 * the file COPYING.
 */

#include "common.h"

#define DISPATCHERS 5
#define THREADS 25
#define ITERS 50

static pthread_mutex_t lock;
static pthread_cond_t cond;
static int running;
static int go;
static char *port;

void *worker(void *arg)
{
	struct mrpc_conn_set *cset=arg;
	struct mrpc_connection *conn;
	int i;
	int ret;

	pthread_mutex_lock(&lock);
	running++;
	pthread_cond_broadcast(&cond);
	while (!go)
		pthread_cond_wait(&cond, &lock);
	pthread_mutex_unlock(&lock);
	for (i=0; i<ITERS; i++) {
		ret=mrpc_conn_create(&conn, cset, NULL);
		if (ret)
			die("%s in mrpc_conn_create() on iteration %d",
						strerror(ret), i);
		ret=mrpc_connect(conn, AF_UNSPEC, NULL, port);
		if (ret)
			die("%s in mrpc_connect() on iteration %d",
						strerror(ret), i);
		sync_client_set_ops(conn);
		sync_client_run(conn);
		trigger_callback_sync(conn);
		invalidate_sync(conn);
		mrpc_conn_close(conn);
		mrpc_conn_unref(conn);
	}
	pthread_mutex_lock(&lock);
	running--;
	pthread_mutex_unlock(&lock);
	pthread_cond_broadcast(&cond);
	return NULL;
}

int main(int argc, char **argv)
{
	struct mrpc_conn_set *sset;
	struct mrpc_conn_set *cset;
	pthread_t thr;
	int i;

	sset=spawn_server(&port, proto_server, sync_server_accept, NULL,
				DISPATCHERS);
	mrpc_set_disconnect_func(sset, disconnect_normal);
	if (mrpc_conn_set_create(&cset, proto_client, NULL))
		die("Couldn't create conn set");
	mrpc_set_disconnect_func(cset, disconnect_user);
	for (i=0; i<DISPATCHERS; i++)
		start_monitored_dispatcher(cset);
	pthread_mutex_init(&lock, NULL);
	pthread_cond_init(&cond, NULL);

	for (i=0; i<THREADS; i++) {
		if (pthread_create(&thr, NULL, worker, cset))
			die("Couldn't create thread");
		if (pthread_detach(thr))
			die("Couldn't detach thread");
	}
	pthread_mutex_lock(&lock);
	while (running < THREADS)
		pthread_cond_wait(&cond, &lock);
	go=1;
	pthread_cond_broadcast(&cond);
	while (running)
		pthread_cond_wait(&cond, &lock);
	pthread_mutex_unlock(&lock);

	mrpc_conn_set_unref(cset);
	mrpc_listen_close(sset);
	mrpc_conn_set_unref(sset);
	expect_disconnects(THREADS * ITERS, THREADS * ITERS, 0);
	free(port);
	return 0;
}
