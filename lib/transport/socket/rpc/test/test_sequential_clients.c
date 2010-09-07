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
	struct mrpc_connection *sconn;
	struct mrpc_connection *conn;
	int ret;
	int i;
	int a, b;

	for (i=0; i<500; i++) {
		get_conn_pair(&a, &b);
		ret=mrpc_conn_create(&sconn, proto_server, a, NULL);
		if (ret)
			die("%s in mrpc_conn_create() on iteration %d",
						strerror(ret), i);
		sync_server_set_ops(sconn);
		start_monitored_dispatcher(sconn);
		ret=mrpc_conn_create(&conn, proto_client, b, NULL);
		if (ret)
			die("%s in mrpc_conn_create() on iteration %d",
						strerror(ret), i);
		sync_client_set_ops(conn);
		sync_client_run(conn);
		mrpc_conn_close(conn);
		mrpc_conn_free(conn);
	}
	dispatcher_barrier();
	return 0;
}
