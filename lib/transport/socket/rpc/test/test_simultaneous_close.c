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

#include <semaphore.h>
#include <glib.h>
#include "common.h"

sem_t serv_can_close;
sem_t serv_closed;
gint server_user_disc;
gint client_user_disc;

void *do_accept(void *set_data, struct mrpc_connection *conn,
			struct sockaddr *from, socklen_t from_len)
{
	sem_wait(&serv_can_close);
	expect(mrpc_conn_close(conn), 0);
	sem_post(&serv_closed);
	mrpc_conn_unref(conn);
	return conn;
}

void common_disconnect(enum mrpc_disc_reason reason, gint *user_ctr)
{
	switch (reason) {
	case MRPC_DISC_USER:
		g_atomic_int_inc(user_ctr);
		break;
	case MRPC_DISC_CLOSED:
		break;
	default:
		die("Received unexpected disconnect reason: %d", reason);
	}
}

void server_disconnect(void *conn_data, enum mrpc_disc_reason reason)
{
	common_disconnect(reason, &server_user_disc);
}

void client_disconnect(void *conn_data, enum mrpc_disc_reason reason)
{
	common_disconnect(reason, &client_user_disc);
}

int main(int argc, char **argv)
{
	struct mrpc_conn_set *sset;
	struct mrpc_conn_set *cset;
	struct mrpc_connection *conn;
	char *port;
	int ret;
	int i;
	int client;
	int server;

	sem_init(&serv_can_close, 0, 0);
	sem_init(&serv_closed, 0, 0);
	sset=spawn_server(&port, proto_server, do_accept, NULL, 1);
	mrpc_set_disconnect_func(sset, server_disconnect);

	if (mrpc_conn_set_create(&cset, proto_client, NULL))
		die("Couldn't allocate conn set");
	mrpc_set_disconnect_func(cset, client_disconnect);
	start_monitored_dispatcher(cset);

	for (i=0; i<ITERS; i++) {
		ret=mrpc_conn_create(&conn, cset, NULL);
		if (ret)
			die("%s", strerror(ret));
		if (i % 2)
			sem_post(&serv_can_close);
		ret=mrpc_connect(conn, AF_UNSPEC, "localhost", port);
		if (ret)
			die("%s", strerror(ret));
		if (i % 2) {
			sem_wait(&serv_closed);
			sem_wait(&serv_closed);
		}
		expect(mrpc_conn_close(conn), 0);
		if (!(i % 2))
			sem_post(&serv_can_close);
		mrpc_conn_unref(conn);
	}

	mrpc_conn_set_unref(cset);
	mrpc_listen_close(sset);
	mrpc_conn_set_unref(sset);
	dispatcher_barrier();
	client=g_atomic_int_get(&client_user_disc);
	server=g_atomic_int_get(&server_user_disc);
	if (client < 0 || client > ITERS)
		die("Client close count = %d, expected 0-%d", client, ITERS);
	if (server < 0 || server > ITERS)
		die("Server close count = %d, expected 0-%d", server, ITERS);
	free(port);
	return 0;
}
