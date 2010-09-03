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
#include <unistd.h>
#include <glib.h>
#include "common.h"

#define MSGPRIV ((void*)0x12345678)

struct {
	pthread_mutex_t lock;
	pthread_cond_t cond;
	int count;
} pending_replies;

struct pending_work {
	struct mrpc_message *msg;
	enum proto_client_procedures proc;

	/* Procedure parameters */
	int integer;
	pthread_cond_t *cond;
	pthread_mutex_t *lock;
};

static GAsyncQueue *pending_work_queue;

static void inc_pending(void)
{
	pthread_mutex_lock(&pending_replies.lock);
	pending_replies.count++;
	pthread_mutex_unlock(&pending_replies.lock);
}

static void dec_pending(void)
{
	pthread_mutex_lock(&pending_replies.lock);
	if (pending_replies.count == 0)
		die("Received more callbacks than we sent");
	pending_replies.count--;
	pthread_mutex_unlock(&pending_replies.lock);
	pthread_cond_broadcast(&pending_replies.cond);
}

static void cb_expect_success(void *conn_private, void *msg_private,
			mrpc_status_t status)
{
	if (status != MINIRPC_OK)
		die("%s: received status %d", msg_private, status);
	dec_pending();
}

static void cb_expect_one(void *conn_private, void *msg_private,
			mrpc_status_t status)
{
	if (status != 1)
		die("%s: received status %d", msg_private, status);
	dec_pending();
}

static void cb_loop(void *conn_private, void *msg_private,
			mrpc_status_t status, struct IntParam *reply)
{
	if (msg_private != MSGPRIV)
		die("Received incorrect msg_private parameter");
	if (status)
		die("Loop RPC returned %d", status);
	if (reply->val != INT_VALUE)
		die("Reply body contained %d", reply->val);
	dec_pending();
}

void loop_int_async(struct mrpc_connection *conn)
{
	struct IntParam request;
	mrpc_status_t ret;

	request.val=INT_VALUE;
	inc_pending();
	ret=proto_loop_int_async(conn, cb_loop, MSGPRIV, &request);
	if (ret)
		die("Couldn't send async loop: %d", ret);
	free_IntParam(&request, 0);
}

void check_int_async(struct mrpc_connection *conn)
{
	struct IntParam request;
	mrpc_status_t ret;

	request.val=INT_VALUE;
	inc_pending();
	ret=proto_check_int_async(conn, cb_expect_success, "Check-correct",
				&request);
	if (ret)
		die("Couldn't send async check: %d", ret);
	free_IntParam(&request, 0);

	request.val=12;
	inc_pending();
	ret=proto_check_int_async(conn, cb_expect_one, "Check-incorrect",
				&request);
	if (ret)
		die("Couldn't send async check: %d", ret);
	free_IntParam(&request, 0);
}

static void cb_error(void *conn_private, void *msg_private,
			mrpc_status_t status, struct IntParam *reply)
{
	if (msg_private != MSGPRIV)
		die("Received incorrect msg_private parameter");
	if (status != 1)
		die("Error received status %d", status);
	if (reply != NULL)
		die("Error callback received non-null reply pointer");
	dec_pending();
}

void error_async(struct mrpc_connection *conn)
{
	mrpc_status_t ret;

	inc_pending();
	ret=proto_error_async(conn, cb_error, MSGPRIV);
	if (ret)
		die("Couldn't send async error: %d", ret);
}

void trigger_callback_async(struct mrpc_connection *conn)
{
	mrpc_status_t ret;

	inc_pending();
	ret=proto_trigger_callback_async(conn, cb_expect_success,
				"Trigger-callback");
	if (ret)
		die("Couldn't send async trigger-callback: %d", ret);
}

static void *process_pending_work(void *unused)
{
	struct pending_work *item;

	g_async_queue_ref(pending_work_queue);
	while (1) {
		item=g_async_queue_pop(pending_work_queue);
		switch (item->proc) {
		case nr_proto_client_check_int:
			if (item->integer == INT_VALUE)
				proto_client_check_int_send_async_reply(
							item->msg);
			else
				proto_client_check_int_send_async_reply_error(
							item->msg, 1);
			break;
		case nr_proto_notify:
			pthread_mutex_lock(item->lock);
			pthread_cond_broadcast(item->cond);
			pthread_mutex_unlock(item->lock);
			break;
		}
		g_slice_free(struct pending_work, item);
	}
	return NULL;
}

static mrpc_status_t client_check_int(void *conn_data,
			struct mrpc_message *msg, IntParam *req)
{
	struct pending_work *work=g_slice_new0(struct pending_work);

	work->msg=msg;
	work->proc=nr_proto_client_check_int;
	work->integer=req->val;
	g_async_queue_push(pending_work_queue, work);
	return MINIRPC_PENDING;
}

static void client_notify(void *conn_data, struct mrpc_message *msg,
			CondVarPtr *req)
{
	struct pending_work *work=g_slice_new0(struct pending_work);

	work->msg=msg;
	work->proc=nr_proto_client_notify;
	work->cond = (void*)(unsigned long)req->cond;
	work->lock = (void*)(unsigned long)req->mutex;
	g_async_queue_push(pending_work_queue, work);
}

static const struct proto_client_operations ops = {
	.client_check_int = client_check_int,
	.client_notify = client_notify
};

void async_client_init(void)
{
	pthread_t thr;

	if (!g_thread_supported())
		g_thread_init(NULL);
	pthread_mutex_init(&pending_replies.lock, NULL);
	pthread_cond_init(&pending_replies.cond, NULL);
	pending_work_queue=g_async_queue_new();
	if (pthread_create(&thr, NULL, process_pending_work, NULL))
		die("Couldn't create worker thread");
}

void async_client_finish(void)
{
	struct timespec ts = {0};
	int ret;

	ts.tv_sec = time(NULL) + FAILURE_TIMEOUT;
	pthread_mutex_lock(&pending_replies.lock);
	while (pending_replies.count) {
		ret=pthread_cond_timedwait(&pending_replies.cond,
					&pending_replies.lock, &ts);
		if (ret == ETIMEDOUT)
			die("Timed out waiting for async ops to finish");
		else if (ret)
			die("Unable to wait on condition variable");
	}
	pthread_mutex_unlock(&pending_replies.lock);
	/* Add some additional delay to notice if we receive more callbacks
	   than we should */
	usleep(50000);
}

void async_client_set_ops(struct mrpc_connection *conn)
{
	int ret;

	ret=proto_client_set_operations(conn, &ops);
	if (ret)
		die("Couldn't set client operations: %d", ret);
}

void async_client_run(struct mrpc_connection *conn)
{
	loop_int_async(conn);
	error_async(conn);
	check_int_async(conn);
}
