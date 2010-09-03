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

void *do_accept(void *set_data, struct mrpc_connection *conn,
			struct sockaddr *from, socklen_t from_len)
{
	expect(mrpc_conn_close(conn), 0);
	return conn;
}

int main(int argc, char **argv)
{
	struct mrpc_conn_set *sset;
	struct mrpc_conn_set *cset;
	struct mrpc_connection *conn;
	char *port;
	int ret;

	sset=spawn_server(&port, proto_server, do_accept, NULL, 1);
	mrpc_set_disconnect_func(sset, disconnect_user_unref);

	if (mrpc_conn_set_create(&cset, proto_client, NULL))
		die("Couldn't allocate conn set");
	start_monitored_dispatcher(cset);

	ret=mrpc_conn_create(&conn, cset, NULL);
	if (ret)
		die("%s", strerror(ret));
	ret=mrpc_connect(conn, AF_UNSPEC, "localhost", port);
	if (ret)
		die("%s", strerror(ret));

	expect(proto_ping(conn), MINIRPC_NETWORK_FAILURE);

	expect(mrpc_conn_close(conn), 0);
	mrpc_conn_unref(conn);
	mrpc_conn_set_unref(cset);
	mrpc_listen_close(sset);
	mrpc_conn_set_unref(sset);
	expect_disconnects(1, 0, 0);
	free(port);
	return 0;
}
