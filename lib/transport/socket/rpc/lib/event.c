/*
 * miniRPC - TCP RPC library with asynchronous operations
 *
 * Copyright (C) 2007-2008 Carnegie Mellon University
 *
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU Lesser General Public License version 2.1, as shown in
 * the file COPYING.
 */

#include <sys/poll.h>
#include <stdarg.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#define MINIRPC_INTERNAL
#include "internal.h"

static pthread_key_t dispatch_thread_data;

struct dispatch_thread_data {
	GQueue *conns;
	struct mrpc_event *active_event;
};

struct dispatch_thread_launch_data {
	struct mrpc_connection *conn;
	sem_t started;
};

struct mrpc_event *mrpc_alloc_event(struct mrpc_connection *conn,
			enum event_type type)
{
	struct mrpc_event *event;

	event=g_slice_new0(struct mrpc_event);
	event->type=type;
	event->conn=conn;
	return event;
}

struct mrpc_event *mrpc_alloc_message_event(struct mrpc_message *msg,
			enum event_type type)
{
	struct mrpc_event *event;

	event=mrpc_alloc_event(msg->conn, type);
	event->msg=msg;
	return event;
}

static void destroy_event(struct mrpc_event *event)
{
	if (event->msg)
		mrpc_free_message(event->msg);
	g_slice_free(struct mrpc_event, event);
}

static int conn_is_plugged(struct mrpc_connection *conn)
{
	return conn->plugged_event != NULL;
}

/* conn->events_lock must be held */
static void update_notify_pipe(struct mrpc_connection *conn)
{
	if ((!conn_is_plugged(conn) && !g_queue_is_empty(conn->events)) ||
				selfpipe_is_set(conn->shutdown_pipe))
		selfpipe_set(conn->events_notify_pipe);
	else
		selfpipe_clear(conn->events_notify_pipe);
}

void queue_event(struct mrpc_event *event)
{
	struct mrpc_connection *conn=event->conn;

	pthread_mutex_lock(&conn->events_lock);
	g_queue_push_tail(conn->events, event);
	conn->events_pending++;
	update_notify_pipe(conn);
	pthread_mutex_unlock(&conn->events_lock);
}

static struct mrpc_event *unqueue_event(struct mrpc_connection *conn)
{
	struct mrpc_event *event=NULL;

	pthread_mutex_lock(&conn->events_lock);
	if (!conn_is_plugged(conn) && !g_queue_is_empty(conn->events)) {
		event=g_queue_pop_head(conn->events);
		conn->plugged_event=event;
	}
	update_notify_pipe(conn);
	pthread_mutex_unlock(&conn->events_lock);
	return event;
}

void kick_event_shutdown_sequence(struct mrpc_connection *conn)
{
	struct mrpc_event *event;

	pthread_mutex_lock(&conn->sequence_lock);
	pthread_mutex_lock(&conn->events_lock);
	if (conn->events_pending == 0 &&
				(conn->sequence_flags & SEQ_FD_CLOSED) &&
				!(conn->sequence_flags & SEQ_PENDING_INIT)) {
		conn->sequence_flags |= SEQ_PENDING_INIT;
		pthread_mutex_unlock(&conn->events_lock);
		pthread_mutex_unlock(&conn->sequence_lock);
		pending_kill(conn);
		pthread_mutex_lock(&conn->sequence_lock);
		pthread_mutex_lock(&conn->events_lock);
		conn->sequence_flags |= SEQ_PENDING_DONE;
	}
	if (conn->events_pending == 0 &&
				(conn->sequence_flags & SEQ_PENDING_DONE) &&
				!(conn->sequence_flags & SEQ_DISC_FIRED)) {
		conn->sequence_flags |= SEQ_DISC_FIRED;
		pthread_mutex_unlock(&conn->events_lock);
		pthread_mutex_unlock(&conn->sequence_lock);
		event=mrpc_alloc_event(conn, EVENT_DISCONNECT);
		queue_event(event);
	} else {
		pthread_mutex_unlock(&conn->events_lock);
		pthread_mutex_unlock(&conn->sequence_lock);
	}
}

static void finish_event(struct mrpc_connection *conn)
{
	int count;

	pthread_mutex_lock(&conn->events_lock);
	assert(conn->events_pending > 0);
	count=--conn->events_pending;
	pthread_mutex_unlock(&conn->events_lock);
	if (!count)
		kick_event_shutdown_sequence(conn);
}

static int _mrpc_release_event(struct mrpc_event *event)
{
	struct mrpc_connection *conn=event->conn;

	pthread_mutex_lock(&conn->events_lock);
	if (conn->plugged_event == NULL || conn->plugged_event != event) {
		pthread_mutex_unlock(&conn->events_lock);
		return ENOENT;
	}
	conn->plugged_event=NULL;
	update_notify_pipe(conn);
	pthread_mutex_unlock(&conn->events_lock);
	return 0;
}

exported int mrpc_get_event_fd(struct mrpc_connection *conn)
{
	return selfpipe_fd(conn->events_notify_pipe);
}

static void fail_request(struct mrpc_event *event, mrpc_status_t err)
{
	struct mrpc_message *request=event->msg;

	_mrpc_release_event(event);
	if (mrpc_send_reply_error(request->conn->protocol,
				request->hdr.cmd, request, err))
		mrpc_free_message(request);
}

static void dispatch_request(struct mrpc_event *event)
{
	struct mrpc_connection *conn=event->conn;
	struct mrpc_message *request=event->msg;
	const void *ops;
	void *request_data;
	void *reply_data=NULL;
	mrpc_status_t ret;
	mrpc_status_t result;
	xdrproc_t request_type;
	xdrproc_t reply_type;
	unsigned reply_size;

	assert(request->hdr.status == MINIRPC_PENDING);

	if (conn->protocol->receiver_request_info(request->hdr.cmd,
				&request_type, NULL)) {
		/* Unknown opcode */
		fail_request(event, MINIRPC_PROCEDURE_UNAVAIL);
		return;
	}

	if (conn->protocol->receiver_reply_info(request->hdr.cmd,
				&reply_type, &reply_size)) {
		/* Can't happen if the info tables are well-formed */
		fail_request(event, MINIRPC_ENCODING_ERR);
		return;
	}
	reply_data=mrpc_alloc_argument(reply_size);
	ret=unformat_request(request, &request_data);
	if (ret) {
		/* Invalid datalen, etc. */
		fail_request(event, ret);
		mrpc_free_argument(NULL, reply_data);
		return;
	}
	/* We don't need the serialized request data anymore.  The request
	   struct may stay around for a while, so free up some memory. */
	mrpc_free_message_data(request);

	assert(conn->protocol->request != NULL);
	ops=g_atomic_pointer_get(&conn->operations);
	result=conn->protocol->request(ops, conn->private,
				request->hdr.cmd, request_data, reply_data);
	_mrpc_release_event(event);
	mrpc_free_argument(request_type, request_data);

	if (result)
		ret=mrpc_send_reply_error(conn->protocol,
					request->hdr.cmd, request,
					result == MINIRPC_PENDING ?
					MINIRPC_ENCODING_ERR :
					result);
	else
		ret=mrpc_send_reply(conn->protocol, request->hdr.cmd, request,
					reply_data);
	mrpc_free_argument(reply_type, reply_data);
	if (ret) {
		g_message("Synchronous reply failed, seq %u cmd %d status %d "
					"err %d", request->hdr.sequence,
					request->hdr.cmd, result, ret);
		mrpc_free_message(request);
	}
}

static void dispatch_event(struct mrpc_event *event)
{
	struct mrpc_connection *conn=event->conn;
	struct dispatch_thread_data *tdata;
	mrpc_disconnect_fn *disconnect;
	int squash;
	int fire_disconnect;
	enum mrpc_disc_reason reason;

	tdata=pthread_getspecific(dispatch_thread_data);
	assert(tdata->active_event == NULL);
	tdata->active_event=event;
	conn_get(conn);
	pthread_mutex_lock(&conn->sequence_lock);
	squash=conn->sequence_flags & SEQ_SQUASH_EVENTS;
	fire_disconnect=conn->sequence_flags & SEQ_HAVE_FD;
	reason=conn->disc_reason;
	pthread_mutex_unlock(&conn->sequence_lock);

	if (squash) {
		switch (event->type) {
		case EVENT_REQUEST:
			_mrpc_release_event(event);
			destroy_event(event);
			goto out;
		default:
			break;
		}
	}

	switch (event->type) {
	case EVENT_REQUEST:
		dispatch_request(event);
		break;
	case EVENT_DISCONNECT:
		disconnect=get_config(conn, disconnect);
		if (fire_disconnect && disconnect)
			disconnect(conn->private, reason);
		conn_put(conn);
		break;
	default:
		assert(0);
	}
	_mrpc_release_event(event);
	g_slice_free(struct mrpc_event, event);
out:
	finish_event(conn);
	conn_put(conn);
	assert(tdata->active_event == event);
	tdata->active_event=NULL;
}

void destroy_events(struct mrpc_connection *conn)
{
	struct mrpc_event *event;

	pthread_mutex_lock(&conn->events_lock);
	while ((event=g_queue_pop_head(conn->events)) != NULL)
		destroy_event(event);
	pthread_mutex_unlock(&conn->events_lock);
}

exported void mrpc_dispatcher_add(struct mrpc_connection *conn)
{
	struct dispatch_thread_data *tdata;

	if (conn == NULL)
		return;
	pthread_mutex_lock(&conn->events_lock);
	conn->events_threads++;
	pthread_mutex_unlock(&conn->events_lock);
	tdata=pthread_getspecific(dispatch_thread_data);
	if (tdata == NULL) {
		tdata=g_slice_new0(struct dispatch_thread_data);
		tdata->conns=g_queue_new();
		pthread_setspecific(dispatch_thread_data, tdata);
	}
	g_queue_push_tail(tdata->conns, conn);
}

exported void mrpc_dispatcher_remove(struct mrpc_connection *conn)
{
	struct dispatch_thread_data *tdata;

	if (conn == NULL)
		return;
	tdata=pthread_getspecific(dispatch_thread_data);
	if (tdata == NULL)
		return;
	assert(tdata->active_event == NULL ||
				tdata->active_event->conn != conn);
	g_queue_remove(tdata->conns, conn);
	if (g_queue_is_empty(tdata->conns)) {
		g_queue_free(tdata->conns);
		g_slice_free(struct dispatch_thread_data, tdata);
		pthread_setspecific(dispatch_thread_data, NULL);
	}
	pthread_mutex_lock(&conn->events_lock);
	conn->events_threads--;
	pthread_cond_broadcast(&conn->events_threads_cond);
	pthread_mutex_unlock(&conn->events_lock);
}

static int mrpc_dispatch_validate(struct mrpc_connection *conn)
{
	struct dispatch_thread_data *tdata;

	if (conn == NULL)
		return EINVAL;
	tdata=pthread_getspecific(dispatch_thread_data);
	if (tdata == NULL || g_queue_find(tdata->conns, conn) == NULL)
		return EPERM;
	return 0;
}

static int mrpc_dispatch_one(struct mrpc_connection *conn)
{
	struct mrpc_event *event;

	event=unqueue_event(conn);
	if (event != NULL)
		dispatch_event(event);
	if (selfpipe_is_set(conn->shutdown_pipe))
		return ENXIO;
	else if (event)
		return 0;
	else
		return EAGAIN;
}

exported int mrpc_dispatch(struct mrpc_connection *conn, int max)
{
	int i;
	int ret;

	ret=mrpc_dispatch_validate(conn);
	if (ret)
		return ret;
	for (i=0; i < max || max == 0; i++) {
		ret=mrpc_dispatch_one(conn);
		if (ret)
			return ret;
	}
	return 0;
}

exported int mrpc_dispatch_loop(struct mrpc_connection *conn)
{
	int ret;

	ret=mrpc_dispatch_validate(conn);
	if (ret)
		return ret;
	while (1) {
		ret=mrpc_dispatch_one(conn);
		if (ret == EAGAIN)
			selfpipe_wait(conn->events_notify_pipe);
		else if (ret)
			break;
	}
	return ret;
}

static void *dispatch_thread(void *arg)
{
	struct dispatch_thread_launch_data *data=arg;
	struct mrpc_connection *conn=data->conn;

	block_signals();
	mrpc_dispatcher_add(conn);
	sem_post(&data->started);
	mrpc_dispatch_loop(conn);
	mrpc_dispatcher_remove(conn);
	return NULL;
}

exported int mrpc_start_dispatch_thread(struct mrpc_connection *conn)
{
	struct dispatch_thread_launch_data data = {0};
	pthread_t thr;
	pthread_attr_t attr;
	int ret;

	if (conn == NULL)
		return EINVAL;
	data.conn=conn;
	ret=sem_init(&data.started, 0, 0);
	if (ret)
		return ret;
	ret=pthread_attr_init(&attr);
	if (ret)
		goto out_sem;
	ret=pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	if (ret)
		goto out_attr;
	ret=pthread_create(&thr, &attr, dispatch_thread, &data);
	if (ret)
		goto out_attr;
	while (sem_wait(&data.started) && errno == EINTR);
out_attr:
	pthread_attr_destroy(&attr);
out_sem:
	sem_destroy(&data.started);
	return ret;
}

void mrpc_event_threadlocal_init(void)
{
	if (pthread_key_create(&dispatch_thread_data, NULL))
		assert(0);
}