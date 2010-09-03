/*
 * miniRPC - TCP RPC library with asynchronous operations
 *
 * Copyright (C) 2007-2008 Carnegie Mellon University
 *
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU Lesser General Public License version 2.1, as shown in
 * the file COPYING.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include "common.h"

int main(int argc, char **argv)
{
	struct mrpc_conn_set *sset;
	struct mrpc_conn_set *cset;
	struct mrpc_connection *conn;
	int sock[2];
	int ret;

	if (mrpc_conn_set_create(&sset, proto_server, NULL))
		die("Couldn't allocate conn set");
	mrpc_set_disconnect_func(sset, disconnect_normal);
	start_monitored_dispatcher(sset);
	if (mrpc_conn_set_create(&cset, proto_client, NULL))
		die("Couldn't allocate conn set");
	mrpc_set_disconnect_func(cset, disconnect_user);
	start_monitored_dispatcher(cset);

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sock))
		die("%s", strerror(errno));

	ret=mrpc_conn_create(&conn, sset, NULL);
	if (ret)
		die("%s", strerror(ret));
	ret=mrpc_bind_fd(conn, sock[0]);
	if (ret)
		die("%s", strerror(ret));
	sync_server_set_ops(conn);
	ret=mrpc_conn_create(&conn, cset, NULL);
	if (ret)
		die("%s", strerror(ret));
	ret=mrpc_bind_fd(conn, sock[1]);
	if (ret)
		die("%s", strerror(ret));
	sync_client_set_ops(conn);

	sync_client_run(conn);
	trigger_callback_sync(conn);
	mrpc_conn_close(conn);
	mrpc_conn_unref(conn);
	mrpc_conn_set_unref(cset);
	mrpc_listen_close(sset);
	mrpc_conn_set_unref(sset);
	expect_disconnects(1, 1, 0);
	return 0;
}
