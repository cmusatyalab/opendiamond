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
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#define MINIRPC_INTERNAL
#include "internal.h"

static int setsockoptval(int fd, int level, int optname, int value)
{
	if (optname == -1) /* optname not supported */
		return 0;
	if (setsockopt(fd, level, optname, &value, sizeof(value)))
		return errno;
	return 0;
}

/* Returns error code if the connection should be killed */
static mrpc_status_t process_incoming_header(struct mrpc_connection *conn,
			void *in, struct mrpc_message *out)
{
	mrpc_status_t ret;

	ret=unserialize((xdrproc_t)xdr_mrpc_header, in, MINIRPC_HEADER_LEN,
				&out->hdr, sizeof(out->hdr));
	if (ret) {
		g_message("Header deserialize failure");
		/* We are now desynchronized */
		return MINIRPC_ENCODING_ERR;
	}
	if (out->hdr.datalen)
		mrpc_alloc_message_data(out, out->hdr.datalen);
	return MINIRPC_OK;
}

mrpc_status_t receive_message(struct mrpc_connection *conn,
			struct mrpc_message **_msg)
{
	char hdr_buf[MINIRPC_HEADER_LEN];
	gboolean want_body = FALSE;
	struct mrpc_message *msg;
	unsigned remaining;
	char *buf;
	ssize_t rcount;
	mrpc_status_t ret;

	*_msg = NULL;
	msg = mrpc_alloc_message(conn);
	remaining = MINIRPC_HEADER_LEN;
	while (1) {
		if (!want_body)
			buf = hdr_buf + MINIRPC_HEADER_LEN - remaining;
		else
			buf = msg->data + msg->hdr.datalen - remaining;

		if (remaining) {
			rcount = read(conn->fd, buf, remaining);
			if (rcount <= 0) {
				if (rcount == -1 && errno != EAGAIN &&
							errno != EINTR)
					g_message("Error %d on read", errno);
				mrpc_free_message(msg);
				mrpc_conn_close(conn);
				return MINIRPC_NETWORK_FAILURE;
			}
			remaining -= rcount;
		}

		if (!remaining) {
			if (!want_body) {
				ret = process_incoming_header(conn, hdr_buf,
							msg);
				if (ret) {
					mrpc_free_message(msg);
					mrpc_conn_close(conn);
					return ret;
				}
				remaining = msg->hdr.datalen;
				want_body = TRUE;
			} else {
				*_msg = msg;
				return MINIRPC_OK;
			}
		}
	}
}

mrpc_status_t send_message(struct mrpc_message *msg)
{
	struct mrpc_connection *conn = msg->conn;
	char hdr_buf[MINIRPC_HEADER_LEN];
	unsigned offset;
	ssize_t rcount;
	char *buf;
	unsigned len;
	int more;
	gboolean send_body = FALSE;

	offset = 0;
	while (1) {
		if (serialize((xdrproc_t)xdr_mrpc_header, &msg->hdr,
					hdr_buf, MINIRPC_HEADER_LEN)) {
			g_message("Header serialize failure");
			mrpc_free_message(msg);
			return MINIRPC_ENCODING_ERR;
		}

		if (!send_body) {
			buf = hdr_buf;
			len = MINIRPC_HEADER_LEN;
			more = msg->hdr.datalen > 0 ? MSG_MORE : 0;
		} else {
			buf = msg->data;
			len = msg->hdr.datalen;
			more = 0;
		}

		if (offset < len) {
			rcount = send(conn->fd, buf + offset, len - offset,
						MSG_NOSIGNAL | more);
			if (rcount <= 0) {
				if (rcount == -1 && errno != EAGAIN
							&& errno != EINTR
							&& errno != EPIPE
							&& errno != ENOTCONN)
					g_message("Error %d on write", errno);
				mrpc_free_message(msg);
				mrpc_conn_close(conn);
				return MINIRPC_NETWORK_FAILURE;
			}
			offset += rcount;
		}

		if (offset == len) {
			if (!send_body) {
				send_body = TRUE;
				offset = 0;
			} else {
				mrpc_free_message(msg);
				return MINIRPC_OK;
			}
		}
	}
}

exported int mrpc_conn_create(struct mrpc_connection **new_conn,
			const struct mrpc_protocol *protocol, int fd,
			void *data)
{
	struct mrpc_connection *conn;
	struct sockaddr_storage sa;
	int type;
	socklen_t len;
	int ret;

	if (new_conn == NULL)
		return EINVAL;
	*new_conn=NULL;
	if (protocol == NULL)
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

	if (sa.ss_family == AF_INET || sa.ss_family == AF_INET6) {
		ret=setsockoptval(fd, IPPROTO_TCP, TCP_KEEPIDLE, 7200);
		if (ret)
			return ret;
		ret=setsockoptval(fd, IPPROTO_TCP, TCP_KEEPCNT, 9);
		if (ret)
			return ret;
		ret=setsockoptval(fd, IPPROTO_TCP, TCP_KEEPINTVL, 75);
		if (ret)
			return ret;
		ret=setsockoptval(fd, IPPROTO_TCP, TCP_NODELAY, 1);
		if (ret)
			return ret;
	}
	ret=setsockoptval(fd, SOL_SOCKET, SO_KEEPALIVE, 1);
	if (ret)
		return ret;
	ret=set_blocking(fd);
	if (ret)
		return ret;
	ret=set_cloexec(fd);
	if (ret)
		return ret;

	conn=g_slice_new0(struct mrpc_connection);
	conn->protocol=protocol;
	conn->private = (data != NULL) ? data : conn;
	conn->fd=fd;
	*new_conn=conn;
	return 0;
}

exported int mrpc_conn_close(struct mrpc_connection *conn)
{
	if (conn == NULL)
		return EINVAL;
	if (shutdown(conn->fd, SHUT_RDWR))
		return errno;
	return 0;
}

exported void mrpc_conn_free(struct mrpc_connection *conn)
{
	if (conn == NULL)
		return;
	close(conn->fd);
	g_slice_free(struct mrpc_connection, conn);
}

exported int mrpc_conn_set_operations(struct mrpc_connection *conn,
			const struct mrpc_protocol *protocol, const void *ops)
{
	if (conn == NULL || conn->protocol != protocol)
		return EINVAL;
	conn->operations = ops;
	return 0;
}
