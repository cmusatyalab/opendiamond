/*
 * miniRPC - Simple TCP RPC library
 *
 * Copyright (C) 2007-2008 Carnegie Mellon University
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#include "common.h"

static mrpc_status_t do_ping(void *conn_data)
{
	return MINIRPC_OK;
}

static mrpc_status_t do_loop_int(void *conn_data, IntParam *in, IntParam *out)
{
	out->val=in->val;
	return MINIRPC_OK;
}

static mrpc_status_t do_check_int(void *conn_data, IntParam *req)
{
	if (req->val == INT_VALUE)
		return MINIRPC_OK;
	else
		return 1;
}

static mrpc_status_t do_error(void *conn_data, IntParam *out)
{
	return 1;
}

static mrpc_status_t do_invalidate_ops(void *conn_data)
{
	if (proto_server_set_operations(conn_data, NULL))
		die("Couldn't set operations");
	return MINIRPC_OK;
}

static mrpc_status_t do_trigger_callback(void *conn_data)
{
	struct mrpc_connection *conn=conn_data;
	struct IntParam ip;
	mrpc_status_t ret;

	ip.val=INT_VALUE;
	ret=proto_client_check_int(conn, &ip);
	if (ret)
		die("Check returned %d", ret);
	free_IntParam(&ip, 0);

	ip.val=12;
	ret=proto_client_check_int(conn, &ip);
	if (ret != 1)
		die("Failed check returned %d", ret);
	free_IntParam(&ip, 0);

	return MINIRPC_OK;
}

static mrpc_status_t do_send_buffer(void *conn_data, KBuffer *in)
{
	return MINIRPC_OK;
}

static mrpc_status_t do_recv_buffer(void *conn_data, KBuffer *out)
{
	return MINIRPC_OK;
}

static const struct proto_server_operations ops = {
	.ping = do_ping,
	.loop_int = do_loop_int,
	.check_int = do_check_int,
	.error = do_error,
	.invalidate_ops = do_invalidate_ops,
	.trigger_callback = do_trigger_callback,
	.send_buffer = do_send_buffer,
	.recv_buffer = do_recv_buffer,
};

void sync_server_set_ops(struct mrpc_connection *conn)
{
	if (proto_server_set_operations(conn, &ops))
		die("Error setting operations struct");
}
