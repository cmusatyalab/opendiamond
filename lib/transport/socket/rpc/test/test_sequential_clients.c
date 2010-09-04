/*
 * miniRPC - TCP RPC library with asynchronous operations
 *
 * Copyright (C) 2007-2010 Carnegie Mellon University
 *
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU Lesser General Public License version 2.1, as shown in
 * the file COPYING.
 */

#include "common.h"

int main(int argc, char **argv)
{
	struct mrpc_conn_set *sset;
	struct mrpc_conn_set *cset;
	struct mrpc_connection *sconn;
	struct mrpc_connection *conn;
	int ret;
	int i;

	if (mrpc_conn_set_create(&sset, proto_server, NULL))
		die("Couldn't create conn set");
	start_monitored_dispatcher(sset);
	mrpc_set_disconnect_func(sset, disconnect_normal);

	if (mrpc_conn_set_create(&cset, proto_client, NULL))
		die("Couldn't create conn set");
	mrpc_set_disconnect_func(cset, disconnect_user);
	start_monitored_dispatcher(cset);

	/* Try repeated connections from the same conn set */
	for (i=0; i<500; i++) {
		ret=mrpc_conn_create(&sconn, sset, NULL);
		if (ret)
			die("%s in mrpc_conn_create() on iteration %d",
						strerror(ret), i);
		sync_server_set_ops(sconn);
		ret=mrpc_conn_create(&conn, cset, NULL);
		if (ret)
			die("%s in mrpc_conn_create() on iteration %d",
						strerror(ret), i);
		bind_conn_pair(sconn, conn);
		sync_client_set_ops(conn);
		sync_client_run(conn);
		mrpc_conn_close(conn);
		mrpc_conn_unref(conn);
	}
	mrpc_conn_set_unref(cset);

	/* Try repeated connections from different conn sets */
	for (i=0; i<100; i++) {
		if (mrpc_conn_set_create(&cset, proto_client, NULL))
			die("Couldn't create conn set");
		mrpc_set_disconnect_func(cset, disconnect_user);
		start_monitored_dispatcher(cset);

		ret=mrpc_conn_create(&sconn, sset, NULL);
		if (ret)
			die("%s in mrpc_conn_create() on iteration %d",
						strerror(ret), i);
		sync_server_set_ops(sconn);
		ret=mrpc_conn_create(&conn, cset, NULL);
		if (ret)
			die("%s in mrpc_conn_create() on iteration %d",
						strerror(ret), i);
		bind_conn_pair(conn, sconn);
		sync_client_set_ops(conn);
		sync_client_run(conn);
		mrpc_conn_close(conn);
		mrpc_conn_unref(conn);
		mrpc_conn_set_unref(cset);
	}

	mrpc_conn_set_unref(sset);
	expect_disconnects(600, 600, 0);
	return 0;
}
