/*
 * miniRPC - TCP RPC library with asynchronous operations
 *
 * Copyright (C) 2007-2008 Carnegie Mellon University
 *
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU Lesser General Public License version 2.1, as shown in
 * the file COPYING.
 */

#define ITERS 100
#define THREADS 5
#define YIELD_TIMEOUT_MS 5

#include <sys/time.h>
#include <time.h>
#include <semaphore.h>
#include <unistd.h>
#include <pthread.h>
#include "common.h"

struct data {
	pthread_mutex_t lock;
	pthread_cond_t yield;
	struct mrpc_connection *conn;
	int serial;
	int current;
	int highwater;
	int parallel;
	int blocked;
};

struct data server;
struct data client;

sem_t complete;

const struct timeval tvd = {
	.tv_sec = 0,
	.tv_usec = 1000 * YIELD_TIMEOUT_MS
};

void yield(struct data *data)
{
	struct timeval tv;
	struct timespec ts;
	int serial;

	gettimeofday(&tv, NULL);
	timeradd(&tv, &tvd, &tv);
	ts.tv_sec=tv.tv_sec;
	ts.tv_nsec = 1000 * tv.tv_usec;
	pthread_mutex_lock(&data->lock);
	serial=++data->serial;
	pthread_cond_broadcast(&data->yield);
	while (data->serial == serial &&
				pthread_cond_timedwait(&data->yield,
				&data->lock, &ts) != ETIMEDOUT);
	pthread_mutex_unlock(&data->lock);
}

void handler_wrapper(struct data *data)
{
	struct mrpc_connection *conn;
	int parallel;

	pthread_mutex_lock(&data->lock);
	conn=data->conn;
	if (data->blocked) {
		if (data == &server)
			die("Request function run when events blocked");
		else
			die("Callback function run when events blocked");
	}
	expect(mrpc_start_events(conn), EINVAL);
	data->current++;
	if (data->highwater < data->current)
		data->highwater=data->current;
	parallel=data->blocked=data->parallel;
	pthread_mutex_unlock(&data->lock);

	if (parallel) {
		expect(mrpc_stop_events(conn), 0);
		expect(mrpc_release_event(), 0);
		expect(mrpc_release_event(), ENOENT);
	}
	yield(data);
	if (parallel) {
		pthread_mutex_lock(&data->lock);
		data->blocked=0;
		pthread_mutex_unlock(&data->lock);
		expect(mrpc_start_events(conn), 0);
		yield(data);
	}

	pthread_mutex_lock(&data->lock);
	data->current--;
	pthread_mutex_unlock(&data->lock);
}

mrpc_status_t do_ping(void *conn_data, struct mrpc_message *msg)
{
	handler_wrapper(&server);
	return MINIRPC_OK;
}

const struct proto_server_operations ops = {
	.ping = do_ping
};

void *do_accept(void *set_data, struct mrpc_connection *conn,
			struct sockaddr *from, socklen_t from_len)
{
	if (proto_server_set_operations(conn, &ops))
		die("Error setting operations struct");
	pthread_mutex_lock(&server.lock);
	server.conn=conn;
	pthread_mutex_unlock(&server.lock);
	return conn;
}

void ping_cb(void *conn_private, void *msg_private, mrpc_status_t status)
{
	expect(status, MINIRPC_OK);
	handler_wrapper(&client);
	sem_post(&complete);
}

void do_round(int cli_pl, int srv_pl)
{
	struct mrpc_connection *cconn;
	int i;

	pthread_mutex_lock(&client.lock);
	client.current=0;
	client.highwater=0;
	client.parallel=cli_pl;
	cconn=client.conn;
	pthread_mutex_unlock(&client.lock);
	pthread_mutex_lock(&server.lock);
	server.current=0;
	server.highwater=0;
	server.parallel=srv_pl;
	pthread_mutex_unlock(&server.lock);

	for (i=0; i<ITERS; i++)
		expect(proto_ping_async(cconn, ping_cb, NULL), MINIRPC_OK);
	for (i=0; i<ITERS; i++)
		sem_wait(&complete);

	pthread_mutex_lock(&client.lock);
	if (cli_pl && client.highwater == 1)
		die("Expected client parallelism but saw none");
	if (!cli_pl && client.highwater > 1)
		die("Expected no client parallelism but got high water of %d",
					client.highwater);
	pthread_mutex_unlock(&client.lock);
	pthread_mutex_lock(&server.lock);
	if (srv_pl && server.highwater == 1)
		die("Expected server parallelism but saw none");
	if (!srv_pl && server.highwater > 1)
		die("Expected no server parallelism but got high water of %d",
					server.highwater);
	pthread_mutex_unlock(&server.lock);
}

int main(int argc, char **argv)
{
	struct mrpc_conn_set *sset;
	struct mrpc_conn_set *cset;
	struct mrpc_connection *conn;
	char *port;
	int ret;
	int i;
	int j;

	pthread_mutex_init(&server.lock, NULL);
	pthread_mutex_init(&client.lock, NULL);
	pthread_cond_init(&server.yield, NULL);
	pthread_cond_init(&client.yield, NULL);
	expect(sem_init(&complete, 0, 0), 0);
	sset=spawn_server(&port, proto_server, do_accept, NULL, THREADS);
	mrpc_set_disconnect_func(sset, disconnect_normal);

	if (mrpc_conn_set_create(&cset, proto_client, NULL))
		die("Couldn't allocate conn set");
	mrpc_set_disconnect_func(cset, disconnect_user);
	for (i=0; i<THREADS; i++)
		start_monitored_dispatcher(cset);

	ret=mrpc_conn_create(&conn, cset, NULL);
	if (ret)
		die("%s", strerror(ret));
	ret=mrpc_connect(conn, AF_UNSPEC, "localhost", port);
	if (ret)
		die("%s", strerror(ret));
	pthread_mutex_lock(&client.lock);
	client.conn=conn;
	pthread_mutex_unlock(&client.lock);

	for (i=0; i<2; i++)
		for (j=0; j<2; j++)
			do_round(i, j);

	mrpc_conn_close(conn);
	mrpc_conn_unref(conn);
	mrpc_conn_set_unref(cset);
	mrpc_listen_close(sset);
	mrpc_conn_set_unref(sset);
	sem_destroy(&complete);
	expect_disconnects(1, 1, 0);
	free(port);
	return 0;
}
