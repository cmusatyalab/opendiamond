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
#include <semaphore.h>
#include <pthread.h>
#include <glib.h>
#include <assert.h>
#include "common.h"

sem_t accepted;
sem_t ready;
sem_t complete;
struct mrpc_message *last_request;

mrpc_status_t do_ping(void *conn_data, struct mrpc_message *msg)
{
	g_atomic_pointer_set(&last_request, msg);
	return MINIRPC_PENDING;
}

const struct proto_server_operations ops = {
	.ping = do_ping
};

void *do_accept(void *set_data, struct mrpc_connection *conn,
			struct sockaddr *from, socklen_t from_len)
{
	sem_post(&accepted);
	if (proto_server_set_operations(conn, &ops))
		die("Error setting operations struct");
	return conn;
}

void *do_sync_ping(void *connp)
{
	struct mrpc_connection *conn=connp;
	mrpc_status_t ret;

	sem_post(&ready);
	ret=proto_ping(conn);
	if (ret != MINIRPC_NETWORK_FAILURE)
		die("Sync ping received %d", ret);
	sem_post(&complete);
	return NULL;
}

void server_disconnect(void *conn_data, enum mrpc_disc_reason reason)
{
	struct mrpc_message *msg;

	msg=g_atomic_pointer_get(&last_request);
	assert(msg != NULL);
	expect(proto_ping_send_async_reply(msg), MINIRPC_NETWORK_FAILURE);
	disconnect_normal(conn_data, reason);
}

int main(int argc, char **argv)
{
	struct mrpc_conn_set *sset;
	struct mrpc_conn_set *cset;
	struct mrpc_connection *conn;
	char *port;
	int ret;
	pthread_t thr;

	expect(sem_init(&accepted, 0, 0), 0);
	expect(sem_init(&ready, 0, 0), 0);
	expect(sem_init(&complete, 0, 0), 0);
	sset=spawn_server(&port, proto_server, do_accept, NULL, 1);
	mrpc_set_disconnect_func(sset, server_disconnect);

	if (mrpc_conn_set_create(&cset, proto_client, NULL))
		die("Couldn't allocate conn set");
	mrpc_set_disconnect_func(cset, disconnect_user);
	start_monitored_dispatcher(cset);

	ret=mrpc_conn_create(&conn, cset, NULL);
	if (ret)
		die("%s", strerror(ret));
	ret=mrpc_connect(conn, AF_UNSPEC, "localhost", port);
	if (ret)
		die("%s", strerror(ret));

	pthread_create(&thr, NULL, do_sync_ping, conn);
	expect(pthread_detach(thr), 0);
	sem_wait(&ready);
	usleep(500000);
	expect(mrpc_conn_close(conn), 0);
	expect(proto_ping(conn), MINIRPC_NETWORK_FAILURE);
	expect(mrpc_conn_close(conn), EALREADY);
	sem_wait(&complete);
	mrpc_conn_unref(conn);
	mrpc_conn_set_unref(cset);
	sem_wait(&accepted);
	mrpc_listen_close(sset);
	mrpc_conn_set_unref(sset);
	expect_disconnects(1, 1, 0);
	free(port);
	return 0;
}
