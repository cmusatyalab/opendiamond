/*
 * miniRPC - TCP RPC library with asynchronous operations
 *
 * Copyright (C) 2007-2008 Carnegie Mellon University
 *
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU Lesser General Public License version 2.1, as shown in
 * the file COPYING.
 */

#include <glib.h>
#include "common.h"

struct pending_work {
	struct mrpc_message *msg;
	enum proto_server_procedures proc;

	/* Procedure parameters */
	struct mrpc_connection *conn;
	int integer;
	pthread_cond_t *cond;
	pthread_mutex_t *lock;
};

static GAsyncQueue *pending;

static void run_trigger_callback(struct mrpc_connection *conn,
			struct mrpc_message *msg)
{
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

	proto_trigger_callback_send_async_reply(msg);
}

static void *process_pending(void *unused)
{
	struct pending_work *item;
	struct IntParam ip;

	g_async_queue_ref(pending);
	while (1) {
		item=g_async_queue_pop(pending);
		switch (item->proc) {
		case nr_proto_ping:
			proto_ping_send_async_reply(item->msg);
			break;
		case nr_proto_loop_int:
			ip.val=item->integer;
			proto_loop_int_send_async_reply(item->msg, &ip);
			free_IntParam(&ip, 0);
			break;
		case nr_proto_check_int:
			if (item->integer == INT_VALUE)
				proto_check_int_send_async_reply(item->msg);
			else
				proto_check_int_send_async_reply_error(
							item->msg, 1);
			break;
		case nr_proto_error:
			proto_error_send_async_reply_error(item->msg, 1);
			break;
		case nr_proto_invalidate_ops:
			if (proto_server_set_operations(item->conn, NULL))
				die("Couldn't set operations");
			proto_invalidate_ops_send_async_reply(item->msg);
			break;
		case nr_proto_trigger_callback:
			run_trigger_callback(item->conn, item->msg);
			break;
		case nr_proto_notify:
			pthread_mutex_lock(item->lock);
			pthread_cond_broadcast(item->cond);
			pthread_mutex_unlock(item->lock);
			break;
		default:
			die("Unknown pending work type %d", item->proc);
		}
		g_slice_free(struct pending_work, item);
	}
	return NULL;
}

static mrpc_status_t do_ping(void *conn_data, struct mrpc_message *msg)
{
	struct pending_work *work=g_slice_new0(struct pending_work);

	work->msg=msg;
	work->proc=nr_proto_ping;
	g_async_queue_push(pending, work);
	return MINIRPC_PENDING;
}

static mrpc_status_t do_loop_int(void *conn_data, struct mrpc_message *msg,
			IntParam *in, IntParam *out)
{
	struct pending_work *work=g_slice_new0(struct pending_work);

	work->msg=msg;
	work->proc=nr_proto_loop_int;
	work->integer=in->val;
	g_async_queue_push(pending, work);
	return MINIRPC_PENDING;
}

static mrpc_status_t do_check_int(void *conn_data, struct mrpc_message *msg,
			IntParam *req)
{
	struct pending_work *work=g_slice_new0(struct pending_work);

	work->msg=msg;
	work->proc=nr_proto_check_int;
	work->integer=req->val;
	g_async_queue_push(pending, work);
	return MINIRPC_PENDING;
}

static mrpc_status_t do_error(void *conn_data, struct mrpc_message *msg,
			IntParam *out)
{
	struct pending_work *work=g_slice_new0(struct pending_work);

	work->msg=msg;
	work->proc=nr_proto_error;
	g_async_queue_push(pending, work);
	return MINIRPC_PENDING;
}

static mrpc_status_t do_invalidate_ops(void *conn_data,
			struct mrpc_message *msg)
{
	struct pending_work *work=g_slice_new0(struct pending_work);

	work->msg=msg;
	work->proc=nr_proto_invalidate_ops;
	work->conn=conn_data;
	g_async_queue_push(pending, work);
	return MINIRPC_PENDING;
}

static void do_notify(void *conn_data, struct mrpc_message *msg,
			CondVarPtr *req)
{
	struct pending_work *work=g_slice_new0(struct pending_work);

	work->msg=msg;
	work->proc=nr_proto_notify;
	work->cond=(void*)(unsigned long)req->cond;
	work->lock=(void*)(unsigned long)req->mutex;
	g_async_queue_push(pending, work);
}

static mrpc_status_t do_trigger_callback(void *conn_data,
			struct mrpc_message *msg)
{
	struct pending_work *work=g_slice_new0(struct pending_work);

	work->msg=msg;
	work->proc=nr_proto_trigger_callback;
	work->conn=conn_data;
	g_async_queue_push(pending, work);
	return MINIRPC_PENDING;
}

static const struct proto_server_operations ops = {
	.ping = do_ping,
	.loop_int = do_loop_int,
	.check_int = do_check_int,
	.error = do_error,
	.invalidate_ops = do_invalidate_ops,
	.trigger_callback = do_trigger_callback,
	.notify = do_notify,
};

void async_server_init(void)
{
	pthread_t thr;

	if (!g_thread_supported())
		g_thread_init(NULL);
	pending=g_async_queue_new();
	if (pthread_create(&thr, NULL, process_pending, NULL))
		die("Couldn't create runner thread");
}

void async_server_set_ops(struct mrpc_connection *conn)
{
	if (proto_server_set_operations(conn, &ops))
		die("Error setting operations struct");
}

void *async_server_accept(void *set_data, struct mrpc_connection *conn,
			struct sockaddr *from, socklen_t from_len)
{
	async_server_set_ops(conn);
	return conn;
}
