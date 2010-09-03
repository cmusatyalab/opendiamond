/*
 * miniRPC - TCP RPC library with asynchronous operations
 *
 * Copyright (C) 2007-2008 Carnegie Mellon University
 *
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU Lesser General Public License version 2.1, as shown in
 * the file COPYING.
 */

#include <semaphore.h>
#include <pthread.h>
#include <assert.h>
#include <errno.h>
#define MINIRPC_INTERNAL
#include "internal.h"

struct pending_reply {
	unsigned sequence;
	int cmd;
	int async;
	union {
		struct {
			sem_t *sem;
			struct mrpc_message **reply;
		} sync;
		struct {
			reply_callback_fn *callback;
			void *private;
		} async;
	} data;
};

struct mrpc_message *mrpc_alloc_message(struct mrpc_connection *conn)
{
	struct mrpc_message *msg;

	msg=g_slice_new0(struct mrpc_message);
	msg->conn=conn;
	msg->lh_msgs=g_list_prepend(NULL, msg);
	pthread_mutex_lock(&conn->msgs_lock);
	g_queue_push_tail_link(conn->msgs, msg->lh_msgs);
	pthread_mutex_unlock(&conn->msgs_lock);
	return msg;
}

void mrpc_free_message(struct mrpc_message *msg)
{
	struct mrpc_connection *conn=msg->conn;

	mrpc_free_message_data(msg);
	pthread_mutex_lock(&conn->msgs_lock);
	if (msg->lh_msgs)
		g_queue_delete_link(conn->msgs, msg->lh_msgs);
	pthread_mutex_unlock(&conn->msgs_lock);
	g_slice_free(struct mrpc_message, msg);
}

void mrpc_alloc_message_data(struct mrpc_message *msg, unsigned len)
{
	assert(msg->data == NULL);
	msg->data=g_malloc(len);
}

void mrpc_free_message_data(struct mrpc_message *msg)
{
	if (msg->data) {
		g_free(msg->data);
		msg->data=NULL;
	}
}

static struct pending_reply *pending_alloc(struct mrpc_message *request)
{
	struct pending_reply *pending;

	pending=g_slice_new(struct pending_reply);
	pending->sequence=request->hdr.sequence;
	pending->cmd=request->hdr.cmd;
	return pending;
}

/* @msg must have already been validated */
static void pending_dispatch(struct pending_reply *pending,
			struct mrpc_message *msg)
{
	struct mrpc_event *event;

	if (pending->async) {
		event=mrpc_alloc_message_event(msg, EVENT_REPLY);
		event->callback=pending->data.async.callback;
		event->private=pending->data.async.private;
		queue_event(event);
	} else {
		/* We need the memory barrier */
		g_atomic_pointer_set(pending->data.sync.reply, msg);
		sem_post(pending->data.sync.sem);
	}
	pending_free(pending);
}

static mrpc_status_t send_request_pending(struct mrpc_message *request,
			struct pending_reply *pending)
{
	struct mrpc_connection *conn=request->conn;
	mrpc_status_t ret;

	pthread_mutex_lock(&conn->pending_replies_lock);
	g_hash_table_replace(conn->pending_replies, &pending->sequence,
				pending);
	ret=send_message(request);
	if (ret)
		g_hash_table_remove(conn->pending_replies, &pending->sequence);
	pthread_mutex_unlock(&conn->pending_replies_lock);
	return ret;
}

static gboolean _pending_kill(void *key, void *value, void *data)
{
	struct mrpc_connection *conn=data;
	struct pending_reply *pending=value;
	struct mrpc_message *msg;

	msg=mrpc_alloc_message(conn);
	msg->hdr.cmd=pending->cmd;
	msg->hdr.status=MINIRPC_NETWORK_FAILURE;
	pending_dispatch(pending, msg);
	return TRUE;
}

void pending_kill(struct mrpc_connection *conn)
{
	pthread_mutex_lock(&conn->pending_replies_lock);
	g_hash_table_foreach_steal(conn->pending_replies, _pending_kill, conn);
	pthread_mutex_unlock(&conn->pending_replies_lock);
}

void pending_free(struct pending_reply *pending)
{
	g_slice_free(struct pending_reply, pending);
}

exported mrpc_status_t mrpc_send_request(const struct mrpc_protocol *protocol,
			struct mrpc_connection *conn, int cmd, void *in,
			void **out)
{
	struct mrpc_message *request;
	struct mrpc_message *reply;
	struct pending_reply *pending;
	sem_t sem;
	mrpc_status_t ret;
	int squash;

	if (out != NULL)
		*out=NULL;
	if (conn == NULL || cmd <= 0)
		return MINIRPC_INVALID_ARGUMENT;
	if (protocol != conn->set->protocol)
		return MINIRPC_INVALID_PROTOCOL;
	ret=format_request(conn, cmd, in, &request);
	if (ret)
		return ret;
	sem_init(&sem, 0, 0);
	pending=pending_alloc(request);
	pending->async=0;
	pending->data.sync.sem=&sem;
	pending->data.sync.reply=&reply;
	ret=send_request_pending(request, pending);
	if (ret) {
		sem_destroy(&sem);
		return ret;
	}

	while (sem_wait(&sem) && errno == EINTR);
	sem_destroy(&sem);
	reply=g_atomic_pointer_get(&reply);
	pthread_mutex_lock(&conn->sequence_lock);
	squash=conn->sequence_flags & SEQ_SQUASH_EVENTS;
	pthread_mutex_unlock(&conn->sequence_lock);
	if (squash)
		ret=MINIRPC_NETWORK_FAILURE;
	else
		ret=unformat_reply(reply, out);
	mrpc_free_message(reply);
	return ret;
}

exported mrpc_status_t mrpc_send_request_async(
			const struct mrpc_protocol *protocol,
			struct mrpc_connection *conn, int cmd,
			reply_callback_fn *callback, void *private, void *in)
{
	struct mrpc_message *msg;
	struct pending_reply *pending;
	mrpc_status_t ret;

	if (conn == NULL || cmd <= 0 || callback == NULL)
		return MINIRPC_INVALID_ARGUMENT;
	if (protocol != conn->set->protocol)
		return MINIRPC_INVALID_PROTOCOL;
	ret=format_request(conn, cmd, in, &msg);
	if (ret)
		return ret;
	pending=pending_alloc(msg);
	pending->async=1;
	pending->data.async.callback=callback;
	pending->data.async.private=private;
	return send_request_pending(msg, pending);
}

exported mrpc_status_t mrpc_send_request_noreply(
			const struct mrpc_protocol *protocol,
			struct mrpc_connection *conn, int cmd, void *in)
{
	struct mrpc_message *msg;
	mrpc_status_t ret;

	if (conn == NULL || cmd >= 0)
		return MINIRPC_INVALID_ARGUMENT;
	if (protocol != conn->set->protocol)
		return MINIRPC_INVALID_PROTOCOL;
	ret=format_request(conn, cmd, in, &msg);
	if (ret)
		return ret;
	return send_message(msg);
}

exported mrpc_status_t mrpc_send_reply(const struct mrpc_protocol *protocol,
			int cmd, struct mrpc_message *request, void *data)
{
	struct mrpc_message *reply;
	mrpc_status_t ret;

	if (request == NULL || cmd != request->hdr.cmd)
		return MINIRPC_INVALID_ARGUMENT;
	if (protocol != request->conn->set->protocol)
		return MINIRPC_INVALID_PROTOCOL;
	ret=format_reply(request, data, &reply);
	if (ret)
		return ret;
	ret=send_message(reply);
	if (ret)
		return ret;
	mrpc_free_message(request);
	return MINIRPC_OK;
}

exported mrpc_status_t mrpc_send_reply_error(
			const struct mrpc_protocol *protocol, int cmd,
			struct mrpc_message *request, mrpc_status_t status)
{
	struct mrpc_message *reply;
	mrpc_status_t ret;

	if (request == NULL || cmd != request->hdr.cmd ||
				status == MINIRPC_OK ||
				status == MINIRPC_PENDING)
		return MINIRPC_INVALID_ARGUMENT;
	if (protocol != request->conn->set->protocol)
		return MINIRPC_INVALID_PROTOCOL;
	ret=format_reply_error(request, status, &reply);
	if (ret)
		return ret;
	ret=send_message(reply);
	if (ret)
		return ret;
	mrpc_free_message(request);
	return MINIRPC_OK;
}

static void check_reply_header(struct pending_reply *pending,
			struct mrpc_message *msg)
{
	struct mrpc_connection *conn=msg->conn;

	if (msg->recv_error) {
		return;
	} else if (pending->cmd != msg->hdr.cmd) {
		msg->recv_error=MINIRPC_ENCODING_ERR;
		queue_ioerr_event(conn, "Mismatched command field in reply, "
					"seq %u, expected cmd %d, found %d",
					msg->hdr.sequence, pending->cmd,
					msg->hdr.cmd);
	} else if (msg->hdr.status != 0 && msg->hdr.datalen != 0) {
		msg->recv_error=MINIRPC_ENCODING_ERR;
		queue_ioerr_event(conn, "Reply with both error and payload, "
					"seq %u", msg->hdr.sequence);
	}
}

void process_incoming_message(struct mrpc_message *msg)
{
	struct mrpc_connection *conn=msg->conn;
	struct pending_reply *pending;
	struct mrpc_event *event;

	if (msg->hdr.status == MINIRPC_PENDING) {
		event=mrpc_alloc_message_event(msg, EVENT_REQUEST);
		queue_event(event);
	} else {
		pthread_mutex_lock(&conn->pending_replies_lock);
		pending=g_hash_table_lookup(conn->pending_replies,
					&msg->hdr.sequence);
		g_hash_table_steal(conn->pending_replies, &msg->hdr.sequence);
		pthread_mutex_unlock(&conn->pending_replies_lock);
		if (pending == NULL) {
			queue_ioerr_event(conn, "Unmatched reply, seq %u cmd "
					"%d status %d len %u",
					msg->hdr.sequence, msg->hdr.cmd,
					msg->hdr.status, msg->hdr.datalen);
			mrpc_free_message(msg);
		} else {
			check_reply_header(pending, msg);
			pending_dispatch(pending, msg);
		}
	}
}
