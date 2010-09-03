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

static void do_notify(void *conn_data, CondVarPtr *req)
{
	pthread_cond_t *cond = (void*)(unsigned long)req->cond;
	pthread_mutex_t *lock = (void*)(unsigned long)req->mutex;

	pthread_mutex_lock(lock);
	pthread_cond_broadcast(cond);
	pthread_mutex_unlock(lock);
}

static mrpc_status_t do_trigger_callback(void *conn_data)
{
	struct mrpc_connection *conn=conn_data;
	struct IntParam ip;
	struct CondVarPtr cvp;
	struct timespec ts = {0};
	pthread_mutex_t lock;
	pthread_cond_t cond;
	mrpc_status_t ret;
	int rval;

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

	pthread_mutex_init(&lock, NULL);
	pthread_cond_init(&cond, NULL);
	cvp.mutex=(unsigned long)&lock;
	cvp.cond=(unsigned long)&cond;
	pthread_mutex_lock(&lock);
	ret=proto_client_notify(conn, &cvp);
	if (ret)
		die("Notify returned %d", ret);
	free_CondVarPtr(&cvp, 0);
	ts.tv_sec=time(NULL) + FAILURE_TIMEOUT;
	rval=pthread_cond_timedwait(&cond, &lock, &ts);
	if (rval == ETIMEDOUT)
		die("Timed out waiting for notify completion");
	else if (rval)
		die("Condition variable wait failed");
	pthread_mutex_unlock(&lock);

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

static void do_msg_buffer(void *conn_data, KBuffer *in)
{
	return;
}

static const struct proto_server_operations ops = {
	.ping = do_ping,
	.loop_int = do_loop_int,
	.check_int = do_check_int,
	.error = do_error,
	.invalidate_ops = do_invalidate_ops,
	.trigger_callback = do_trigger_callback,
	.notify = do_notify,
	.send_buffer = do_send_buffer,
	.recv_buffer = do_recv_buffer,
	.msg_buffer = do_msg_buffer,
};

void sync_server_set_ops(struct mrpc_connection *conn)
{
	if (proto_server_set_operations(conn, &ops))
		die("Error setting operations struct");
}

void *sync_server_accept(void *set_data, struct mrpc_connection *conn,
			struct sockaddr *from, socklen_t from_len)
{
	sync_server_set_ops(conn);
	return conn;
}
