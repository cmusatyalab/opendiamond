/*
 * miniRPC - TCP RPC library with asynchronous operations
 *
 * Copyright (C) 2007-2008 Carnegie Mellon University
 *
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU Lesser General Public License version 2.1, as shown in
 * the file COPYING.
 */

#include <time.h>
#include "common.h"

void loop_int_sync(struct mrpc_connection *conn)
{
	struct IntParam request;
	struct IntParam *reply;
	mrpc_status_t ret;

	request.val=INT_VALUE;
	ret=proto_loop_int(conn, &request, &reply);
	if (ret)
		die("Loop returned %d", ret);
	if (reply->val != INT_VALUE)
		die("Reply body contained %d", reply->val);
	free_IntParam(&request, 0);
	free_IntParam(reply, 1);
}

void check_int_sync(struct mrpc_connection *conn)
{
	struct IntParam request;
	mrpc_status_t ret;

	request.val=INT_VALUE;
	ret=proto_check_int(conn, &request);
	if (ret)
		die("Check returned %d", ret);
	free_IntParam(&request, 0);

	request.val=12;
	ret=proto_check_int(conn, &request);
	if (ret != 1)
		die("Failed check returned %d", ret);
	free_IntParam(&request, 0);
}

void error_sync(struct mrpc_connection *conn)
{
	struct IntParam *reply=(void*)1;
	mrpc_status_t ret;

	ret=proto_error(conn, &reply);
	if (ret != 1)
		die("Error returned %d", ret);
	if (reply != NULL)
		die("Error did not produce NULL reply pointer");
}

void notify_sync(struct mrpc_connection *conn)
{
	struct CondVarPtr notify;
	struct timespec ts = {0};
	pthread_mutex_t lock;
	pthread_cond_t cond;
	mrpc_status_t ret;
	int rval;

	pthread_mutex_init(&lock, NULL);
	pthread_cond_init(&cond, NULL);
	notify.mutex=(unsigned long)&lock;
	notify.cond=(unsigned long)&cond;
	pthread_mutex_lock(&lock);
	ret=proto_notify(conn, &notify);
	if (ret)
		die("Notify returned %d", ret);
	free_CondVarPtr(&notify, 0);
	ts.tv_sec=time(NULL) + FAILURE_TIMEOUT;
	rval=pthread_cond_timedwait(&cond, &lock, &ts);
	if (rval == ETIMEDOUT)
		die("Timed out waiting for notify completion");
	else if (rval)
		die("Condition variable wait failed");
	pthread_mutex_unlock(&lock);
}

void trigger_callback_sync(struct mrpc_connection *conn)
{
	mrpc_status_t ret;

	ret=proto_trigger_callback(conn);
	if (ret)
		die("Trigger-callback returned %d", ret);
}

void invalidate_sync(struct mrpc_connection *conn)
{
	int ret;

	ret=proto_ping(conn);
	if (ret)
		die("Ping returned %d", ret);
	ret=proto_invalidate_ops(conn);
	if (ret)
		die("Invalidate returned %d", ret);
	ret=proto_ping(conn);
	if (ret != MINIRPC_PROCEDURE_UNAVAIL)
		die("Ping returned %d", ret);
}

mrpc_status_t send_buffer_sync(struct mrpc_connection *conn)
{
	KBuffer buf = {0};

	return proto_send_buffer(conn, &buf);
}

mrpc_status_t recv_buffer_sync(struct mrpc_connection *conn)
{
	KBuffer *buf;
	mrpc_status_t ret;

	ret=proto_recv_buffer(conn, &buf);
	if (!ret)
		free_KBuffer(buf, 1);
	return ret;
}

void msg_buffer_sync(struct mrpc_connection *conn)
{
	KBuffer buf = {0};
	mrpc_status_t ret;

	ret=proto_msg_buffer(conn, &buf);
	if (ret)
		die("msg_buffer returned %d", ret);
}

static mrpc_status_t client_check_int(void *conn_data,
			struct mrpc_message *msg, IntParam *req)
{
	if (req->val == INT_VALUE)
		return MINIRPC_OK;
	else
		return 1;
}

static void client_notify(void *conn_data, struct mrpc_message *msg,
			CondVarPtr *req)
{
	pthread_cond_t *cond = (void*)(unsigned long)req->cond;
	pthread_mutex_t *lock = (void*)(unsigned long)req->mutex;

	pthread_mutex_lock(lock);
	pthread_cond_broadcast(cond);
	pthread_mutex_unlock(lock);
}

static const struct proto_client_operations ops = {
	.client_check_int = client_check_int,
	.client_notify = client_notify
};

void sync_client_set_ops(struct mrpc_connection *conn)
{
	int ret;

	ret=proto_client_set_operations(conn, &ops);
	if (ret)
		die("Couldn't set client operations: %d", ret);
}

void sync_client_run(struct mrpc_connection *conn)
{
	loop_int_sync(conn);
	error_sync(conn);
	check_int_sync(conn);
	notify_sync(conn);
}
