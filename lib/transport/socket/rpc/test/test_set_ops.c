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

#define ITERS 25

#include "common.h"

const struct proto_server_operations ops_ok;
const struct proto_server_operations ops_fail;

static mrpc_status_t do_ping_ok(void *conn_data)
{
	expect(proto_server_set_operations(conn_data, &ops_fail), 0);
	return MINIRPC_OK;
}

static mrpc_status_t do_ping_fail(void *conn_data)
{
	expect(proto_server_set_operations(conn_data, &ops_ok), 0);
	return 1;
}

const struct proto_server_operations ops_ok = {
	.ping = do_ping_ok
};

const struct proto_server_operations ops_fail = {
	.ping = do_ping_fail
};

int main(int argc, char **argv)
{
	struct mrpc_connection *sconn;
	struct mrpc_connection *conn;
	int i;
	int a, b;

	get_conn_pair(&a, &b);
	if (mrpc_conn_create(&sconn, proto_server, a, NULL))
		die("Couldn't allocate conn");
	expect(proto_server_set_operations(sconn, &ops_ok), 0);
	start_monitored_dispatcher(sconn);
	if (mrpc_conn_create(&conn, proto_client, b, NULL))
		die("Couldn't allocate conn");

	for (i=0; i<ITERS; i++) {
		expect(proto_ping(conn), 0);
		expect(proto_ping(conn), 1);
	}

	mrpc_conn_close(conn);
	mrpc_conn_free(conn);
	dispatcher_barrier();
	return 0;
}
