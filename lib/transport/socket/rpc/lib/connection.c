/*
 * miniRPC - TCP RPC library with asynchronous operations
 *
 * Copyright (C) 2007-2008 Carnegie Mellon University
 *
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU Lesser General Public License version 2.1, as shown in
 * the file COPYING.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <pthread.h>
#define MINIRPC_INTERNAL
#include "internal.h"

static void conn_kill(struct mrpc_connection *conn,
			enum mrpc_disc_reason reason);
static int try_close_fd(struct mrpc_connection *conn);
static void *listener(void *data);

static int setsockoptval(int fd, int level, int optname, int value)
{
	if (optname == -1) /* optname not supported */
		return 0;
	if (setsockopt(fd, level, optname, &value, sizeof(value)))
		return errno;
	return 0;
}

#define REFCOUNT_GET_FUNC(modifier, name, type, member)			\
	modifier void name(type *item)					\
	{								\
		gint old;						\
									\
		if (item == NULL)					\
			return;						\
		old=g_atomic_int_exchange_and_add(&item->member, 1);	\
		assert(old > 0);					\
	}
#define REFCOUNT_PUT_FUNC(modifier, name, type, member, cleanup_action)	\
	modifier void name(type *item)					\
	{								\
		gint old;						\
									\
		if (item == NULL)					\
			return;						\
		old=g_atomic_int_exchange_and_add(&item->member, -1);	\
		assert(old > 0);					\
		if (old == 1) {						\
			cleanup_action;					\
		}							\
	}
/* We can't call conn_free() directly from conn_put(): if we were
   called from an event thread, that would deadlock.  Have the listener
   thread clean up the conn. */
REFCOUNT_GET_FUNC(, conn_get, struct mrpc_connection, refs)
REFCOUNT_PUT_FUNC(, conn_put, struct mrpc_connection, refs,
			selfpipe_set(item->shutdown_pipe))
REFCOUNT_GET_FUNC(exported, mrpc_conn_ref, struct mrpc_connection, user_refs)
REFCOUNT_PUT_FUNC(exported, mrpc_conn_unref, struct mrpc_connection, user_refs,
			conn_put(item))
#undef REFCOUNT_GET_FUNC
#undef REFCOUNT_PUT_FUNC

/* Returns error code if the connection should be killed */
static mrpc_status_t process_incoming_header(struct mrpc_connection *conn)
{
	mrpc_status_t ret;

	ret=unserialize((xdrproc_t)xdr_mrpc_header, conn->recv_hdr_buf,
				MINIRPC_HEADER_LEN, &conn->recv_msg->hdr,
				sizeof(conn->recv_msg->hdr));
	if (ret) {
		g_message("Header deserialize failure");
		/* We are now desynchronized */
		return MINIRPC_ENCODING_ERR;
	}

	if (conn->recv_msg->hdr.datalen)
		mrpc_alloc_message_data(conn->recv_msg,
					conn->recv_msg->hdr.datalen);
	return MINIRPC_OK;
}

static void try_read_conn(void *data)
{
	struct mrpc_connection *conn=data;
	size_t count;
	ssize_t rcount;
	char *buf;

	while (1) {
		if (conn->recv_msg == NULL) {
			conn->recv_msg=mrpc_alloc_message(conn);
			conn->recv_remaining=MINIRPC_HEADER_LEN;
		}

		switch (conn->recv_state) {
		case STATE_HEADER:
			buf=conn->recv_hdr_buf + MINIRPC_HEADER_LEN -
						conn->recv_remaining;
			count=conn->recv_remaining;
			break;
		case STATE_DATA:
			buf=conn->recv_msg->data +
						conn->recv_msg->hdr.datalen -
						conn->recv_remaining;
			count=conn->recv_remaining;
			break;
		default:
			assert(0);
			return;  /* Avoid warning */
		}

		if (conn->recv_remaining) {
			assert(!(conn->sequence_flags & SEQ_FD_CLOSED));
			rcount=read(conn->fd, buf, count);
			if (rcount <= 0) {
				if (rcount == 0) {
					conn_kill(conn, MRPC_DISC_CLOSED);
				} else if (errno != EAGAIN && errno != EINTR) {
					g_message("Error %d on read", errno);
					conn_kill(conn, MRPC_DISC_IOERR);
				}
				return;
			}
			conn->recv_remaining -= rcount;
		}

		if (!conn->recv_remaining) {
			switch (conn->recv_state) {
			case STATE_HEADER:
				if (process_incoming_header(conn)) {
					conn_kill(conn, MRPC_DISC_IOERR);
					return;
				}
				conn->recv_remaining =
						conn->recv_msg->hdr.datalen;
				conn->recv_state=STATE_DATA;
				break;
			case STATE_DATA:
				process_incoming_message(conn->recv_msg);
				conn->recv_state=STATE_HEADER;
				conn->recv_msg=NULL;
				break;
			default:
				assert(0);
			}
		}
	}
}

static mrpc_status_t get_next_message(struct mrpc_connection *conn)
{
again:
	pthread_mutex_lock(&conn->send_msgs_lock);
	assert(!(conn->sequence_flags & SEQ_FD_CLOSED));
	if (g_queue_is_empty(conn->send_msgs)) {
		pollset_modify(conn->pollset, conn->fd, POLLSET_READABLE);
		pthread_mutex_unlock(&conn->send_msgs_lock);
		if (try_close_fd(conn))
			return MINIRPC_NETWORK_FAILURE;
		return MINIRPC_OK;
	}
	conn->send_msg=g_queue_pop_head(conn->send_msgs);
	pthread_mutex_unlock(&conn->send_msgs_lock);

	if (serialize((xdrproc_t)xdr_mrpc_header, &conn->send_msg->hdr,
				conn->send_hdr_buf, MINIRPC_HEADER_LEN)) {
		/* Message dropped on floor */
		g_message("Header serialize failure");
		mrpc_free_message(conn->send_msg);
		conn->send_msg=NULL;
		goto again;
	}
	return MINIRPC_OK;
}

/* May give false positives due to races.  Should never give false
   negatives. */
static int send_queue_is_empty(struct mrpc_connection *conn)
{
	int ret;

	pthread_mutex_lock(&conn->send_msgs_lock);
	ret=g_queue_is_empty(conn->send_msgs);
	pthread_mutex_unlock(&conn->send_msgs_lock);
	return ret;
}

static void try_write_conn(void *data)
{
	struct mrpc_connection *conn=data;
	size_t count;
	ssize_t rcount;
	char *buf;
	unsigned len;
	int more;

	while (1) {
		if (conn->send_msg == NULL) {
			/* If get_next_message() returns true, the fd has
			   been closed and the conn handle may already be
			   invalid */
			if (get_next_message(conn) || conn->send_msg == NULL)
				break;
		}

		switch (conn->send_state) {
		case STATE_HEADER:
			buf=conn->send_hdr_buf;
			len=MINIRPC_HEADER_LEN;
			more = conn->send_msg->hdr.datalen > 0 ||
						!send_queue_is_empty(conn);
			break;
		case STATE_DATA:
			buf=conn->send_msg->data;
			len=conn->send_msg->hdr.datalen;
			more = !send_queue_is_empty(conn);
			break;
		default:
			assert(0);
			return;  /* Avoid warning */
		}

		if (conn->send_offset < len) {
			count = len - conn->send_offset;
			assert(!(conn->sequence_flags & SEQ_FD_CLOSED));
			rcount=send(conn->fd, buf + conn->send_offset, count,
						more ? MSG_MORE : 0);
			if (rcount <= 0) {
				if (rcount == -1 && errno != EAGAIN
							&& errno != EINTR) {
					g_message("Error %d on write", errno);
					conn_kill(conn, MRPC_DISC_IOERR);
				}
				return;
			}
			conn->send_offset += rcount;
		}

		if (conn->send_offset == len) {
			switch (conn->send_state) {
			case STATE_HEADER:
				conn->send_state=STATE_DATA;
				conn->send_offset=0;
				break;
			case STATE_DATA:
				conn->send_state=STATE_HEADER;
				conn->send_offset=0;
				mrpc_free_message(conn->send_msg);
				conn->send_msg=NULL;
				break;
			default:
				assert(0);
			}
		}
	}
}

static void conn_hangup(void *data)
{
	struct mrpc_connection *conn=data;

	conn_kill(conn, MRPC_DISC_CLOSED);
}

static void conn_error(void *data)
{
	struct mrpc_connection *conn=data;

	g_message("Poll reported I/O error");
	conn_kill(conn, MRPC_DISC_IOERR);
}

mrpc_status_t send_message(struct mrpc_message *msg)
{
	struct mrpc_connection *conn=msg->conn;
	mrpc_status_t ret=MINIRPC_OK;

	pthread_mutex_lock(&conn->send_msgs_lock);
	pthread_mutex_lock(&conn->sequence_lock);
	if (!(conn->sequence_flags & SEQ_HAVE_FD)) {
		ret=MINIRPC_INVALID_ARGUMENT;
		goto out;
	}
	if ((conn->sequence_flags & SEQ_SHUT_STARTED) ||
				pollset_modify(conn->pollset, conn->fd,
				POLLSET_READABLE|POLLSET_WRITABLE)) {
		ret=MINIRPC_NETWORK_FAILURE;
		goto out;
	}
	g_queue_push_tail(conn->send_msgs, msg);
out:
	pthread_mutex_unlock(&conn->sequence_lock);
	pthread_mutex_unlock(&conn->send_msgs_lock);
	if (ret)
		mrpc_free_message(msg);
	return ret;
}

exported int mrpc_conn_create(struct mrpc_connection **new_conn,
			const struct mrpc_protocol *protocol, void *data)
{
	struct mrpc_connection *conn;
	pthread_attr_t attr;
	pthread_t thr;
	int ret;

	if (new_conn == NULL)
		return EINVAL;
	*new_conn=NULL;
	if (protocol == NULL)
		return EINVAL;
	mrpc_init();
	conn=g_slice_new0(struct mrpc_connection);
	pthread_mutex_init(&conn->config_lock, NULL);
	pthread_mutex_init(&conn->events_lock, NULL);
	pthread_cond_init(&conn->events_threads_cond, NULL);
	conn->protocol=protocol;
	g_atomic_int_set(&conn->refs, 1);
	g_atomic_int_set(&conn->user_refs, 1);
	conn->msgs=g_queue_new();
	conn->send_msgs=g_queue_new();
	conn->events=g_queue_new();
	pthread_mutex_init(&conn->msgs_lock, NULL);
	pthread_mutex_init(&conn->send_msgs_lock, NULL);
	pthread_mutex_init(&conn->pending_replies_lock, NULL);
	pthread_mutex_init(&conn->sequence_lock, NULL);
	conn->send_state=STATE_HEADER;
	conn->recv_state=STATE_HEADER;
	conn->private = (data != NULL) ? data : conn;
	conn->pending_replies=g_hash_table_new_full(g_int_hash, g_int_equal,
				NULL, (GDestroyNotify)pending_free);
	ret=selfpipe_create(&conn->shutdown_pipe);
	if (ret)
		goto bad;
	ret=selfpipe_create(&conn->events_notify_pipe);
	if (ret)
		goto bad;
	ret=pollset_alloc(&conn->pollset);
	if (ret)
		goto bad;
	ret=pollset_add(conn->pollset, selfpipe_fd(conn->shutdown_pipe),
				POLLSET_READABLE, NULL, NULL, NULL, NULL,
				assert_callback_func, NULL);
	if (ret)
		goto bad;
	ret=pthread_attr_init(&attr);
	if (ret)
		goto bad;
	ret=pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	if (ret)
		goto bad;
	ret=pthread_create(&thr, &attr, listener, conn);
	if (ret)
		goto bad;
	pthread_attr_destroy(&attr);

	*new_conn=conn;
	return 0;

bad:
	if (conn->pollset)
		pollset_free(conn->pollset);
	if (conn->events_notify_pipe)
		selfpipe_destroy(conn->events_notify_pipe);
	if (conn->shutdown_pipe)
		selfpipe_destroy(conn->shutdown_pipe);
	g_queue_free(conn->events);
	g_hash_table_destroy(conn->pending_replies);
	g_queue_free(conn->send_msgs);
	g_queue_free(conn->msgs);
	g_slice_free(struct mrpc_connection, conn);
	return ret;
}

/* Sequence lock must be held */
static void conn_start_shutdown(struct mrpc_connection *conn,
			enum mrpc_disc_reason reason)
{
	int done;

	done=conn->sequence_flags & SEQ_SHUT_STARTED;
	conn->sequence_flags |= SEQ_SHUT_STARTED;
	if (!done)
		conn->disc_reason=reason;
}

/* Must be called from listener-thread context.  Returns true if the fd was
   closed. */
static int try_close_fd(struct mrpc_connection *conn)
{
	int do_event=0;

	conn_get(conn);
	pthread_mutex_lock(&conn->sequence_lock);
	if ((conn->sequence_flags & SEQ_SHUT_STARTED) &&
				!(conn->sequence_flags & SEQ_FD_CLOSED)) {
		pollset_del(conn->pollset, conn->fd);
		/* We are now guaranteed that the listener thread will not
		   process this connection further (once the current handler
		   returns) */
		close(conn->fd);
		conn->sequence_flags |= SEQ_FD_CLOSED;
		do_event=1;
	}
	pthread_mutex_unlock(&conn->sequence_lock);
	if (do_event)
		kick_event_shutdown_sequence(conn);
	conn_put(conn);
	return do_event;
}

/* Must be called from listener-thread context */
static void conn_kill(struct mrpc_connection *conn,
			enum mrpc_disc_reason reason)
{
	pthread_mutex_lock(&conn->sequence_lock);
	conn_start_shutdown(conn, reason);
	pthread_mutex_unlock(&conn->sequence_lock);
	/* Squash send queue */
	try_close_fd(conn);
}

exported int mrpc_conn_close(struct mrpc_connection *conn)
{
	int ret=0;

	if (conn == NULL)
		return EINVAL;
	conn_get(conn);
	pthread_mutex_lock(&conn->sequence_lock);
	if (!(conn->sequence_flags & SEQ_HAVE_FD)) {
		ret=ENOTCONN;
		goto out;
	}
	conn_start_shutdown(conn, MRPC_DISC_USER);
	/* Squash event queue */
	if (conn->sequence_flags & SEQ_SQUASH_EVENTS) {
		ret=EALREADY;
		goto out;
	}
	conn->sequence_flags |= SEQ_SQUASH_EVENTS;
	if (!(conn->sequence_flags & SEQ_FD_CLOSED))
		pollset_modify(conn->pollset, conn->fd,
					POLLSET_READABLE | POLLSET_WRITABLE);
out:
	pthread_mutex_unlock(&conn->sequence_lock);
	conn_put(conn);
	return ret;
}

static void conn_free(struct mrpc_connection *conn)
{
	struct mrpc_message *msg;

	selfpipe_set(conn->events_notify_pipe);
	pthread_mutex_lock(&conn->events_lock);
	while (conn->events_threads)
		pthread_cond_wait(&conn->events_threads_cond,
					&conn->events_lock);
	pthread_mutex_unlock(&conn->events_lock);
	destroy_events(conn);
	g_queue_free(conn->events);
	pollset_free(conn->pollset);
	selfpipe_destroy(conn->events_notify_pipe);
	selfpipe_destroy(conn->shutdown_pipe);
	g_hash_table_destroy(conn->pending_replies);
	if (conn->send_msg)
		mrpc_free_message(conn->send_msg);
	if (conn->recv_msg)
		mrpc_free_message(conn->recv_msg);
	while ((msg=g_queue_pop_head(conn->send_msgs)) != NULL)
		mrpc_free_message(msg);
	g_queue_free(conn->send_msgs);
	while ((msg=g_queue_pop_head(conn->msgs)) != NULL) {
		msg->lh_msgs=NULL;
		mrpc_free_message(msg);
	}
	g_queue_free(conn->msgs);
	g_slice_free(struct mrpc_connection, conn);
}

exported int mrpc_bind_fd(struct mrpc_connection *conn, int fd)
{
	struct sockaddr_storage sa;
	int type;
	socklen_t len;
	int ret;

	if (conn == NULL)
		return EINVAL;

	/* Make sure this is a connected socket */
	len=sizeof(sa);
	if (getpeername(fd, (struct sockaddr *)&sa, &len))
		return errno;

	/* Make sure it's SOCK_STREAM */
	len=sizeof(type);
	if (getsockopt(fd, SOL_SOCKET, SO_TYPE, &type, &len))
		return errno;
	if (type != SOCK_STREAM)
		return EPROTONOSUPPORT;

	pthread_mutex_lock(&conn->sequence_lock);
	if (conn->sequence_flags & SEQ_HAVE_FD) {
		ret=EINVAL;
		goto out;
	}
	if (sa.ss_family == AF_INET || sa.ss_family == AF_INET6) {
		ret=setsockoptval(fd, IPPROTO_TCP, TCP_KEEPIDLE, 7200);
		if (ret)
			goto out;
		ret=setsockoptval(fd, IPPROTO_TCP, TCP_KEEPCNT, 9);
		if (ret)
			goto out;
		ret=setsockoptval(fd, IPPROTO_TCP, TCP_KEEPINTVL, 75);
		if (ret)
			goto out;
		ret=setsockoptval(fd, IPPROTO_TCP, TCP_NODELAY, 1);
		if (ret)
			goto out;
	}
	ret=setsockoptval(fd, SOL_SOCKET, SO_KEEPALIVE, 1);
	if (ret)
		goto out;
	ret=set_nonblock(fd);
	if (ret)
		goto out;
	ret=set_cloexec(fd);
	if (ret)
		goto out;
	conn->fd=fd;
	ret=pollset_add(conn->pollset, fd, POLLSET_READABLE, conn,
				try_read_conn, try_write_conn, conn_hangup,
				conn_error, NULL);
	if (!ret) {
		conn->sequence_flags |= SEQ_HAVE_FD;
		conn_get(conn);
	}
out:
	pthread_mutex_unlock(&conn->sequence_lock);
	return ret;
}

exported int mrpc_conn_set_operations(struct mrpc_connection *conn,
			const struct mrpc_protocol *protocol, const void *ops)
{
	if (conn == NULL || conn->protocol != protocol)
		return EINVAL;
	g_atomic_pointer_set(&conn->operations, ops);
	return 0;
}

static void *listener(void *data)
{
	struct mrpc_connection *conn=data;

	block_signals();
	while (!selfpipe_is_set(conn->shutdown_pipe))
		pollset_poll(conn->pollset);
	conn_free(conn);
	return NULL;
}
