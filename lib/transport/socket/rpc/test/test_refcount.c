/*
 * miniRPC - TCP RPC library with asynchronous operations
 *
 * Copyright (C) 2007-2008 Carnegie Mellon University
 *
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU Lesser General Public License version 2.1, as shown in
 * the file COPYING.
 */

#include <semaphore.h>
#include "common.h"

sem_t accepted;

void *do_accept(void *set_data, struct mrpc_connection *conn,
			struct sockaddr *from, socklen_t from_len)
{
	sem_post(&accepted);
	mrpc_conn_unref(conn);
	sync_server_set_ops(conn);
	return conn;
}

int main(int argc, char **argv)
{
	struct mrpc_conn_set *sset;
	struct mrpc_conn_set *cset;
	struct mrpc_connection *conn;
	char *port;
	int ret;

	sem_init(&accepted, 0, 0);
	sset=spawn_server(&port, proto_server, do_accept, NULL, 1);
	mrpc_set_disconnect_func(sset, disconnect_normal_no_unref);
	mrpc_conn_set_ref(sset);

	if (mrpc_conn_set_create(&cset, proto_client, NULL))
		die("Couldn't allocate conn set");
	mrpc_conn_set_ref(cset);
	mrpc_set_disconnect_func(cset, disconnect_user);
	start_monitored_dispatcher(cset);

	ret=mrpc_conn_create(&conn, cset, NULL);
	if (ret)
		die("%s", strerror(ret));
	ret=mrpc_connect(conn, AF_UNSPEC, "localhost", port);
	if (ret)
		die("%s", strerror(ret));
	mrpc_conn_ref(conn);
	mrpc_conn_set_unref(cset);
	mrpc_conn_set_unref(cset);
	mrpc_conn_set_unref(sset);
	mrpc_conn_set_unref(sset);
	sem_wait(&accepted);
	mrpc_listen_close(sset);
	mrpc_conn_unref(conn);
	mrpc_conn_unref(conn);

	sync_client_set_ops(conn);
	sync_client_run(conn);
	trigger_callback_sync(conn);
	invalidate_sync(conn);
	mrpc_conn_close(conn);
	expect_disconnects(1, 1, 0);
	sem_destroy(&accepted);
	free(port);
	return 0;
}
