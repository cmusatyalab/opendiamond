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
