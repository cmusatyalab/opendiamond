/*
 * miniRPC - TCP RPC library with asynchronous operations
 *
 * Copyright (C) 2007-2008 Carnegie Mellon University
 *
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU Lesser General Public License version 2.1, as shown in
 * the file COPYING.
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#define MINIRPC_INTERNAL
#include "internal.h"

void *mrpc_alloc_argument(unsigned len)
{
	void *buf;

	if (len == 0)
		return NULL;
	buf=malloc(len);
	if (buf == NULL)
		abort();
	memset(buf, 0, len);
	return buf;
}

void mrpc_free_argument(xdrproc_t xdr_proc, void *buf)
{
	if (buf == NULL)
		return;
	if (xdr_proc)
		xdr_free(xdr_proc, buf);
	free(buf);
}

static mrpc_status_t serialize_common(enum xdr_op direction, xdrproc_t xdr_proc,
			void *data, char *buf, unsigned buflen)
{
	XDR xdrs;
	mrpc_status_t ret=MINIRPC_OK;

	xdrmem_create(&xdrs, buf, buflen, direction);
	if (!xdr_proc(&xdrs, data) || xdr_getpos(&xdrs) != buflen)
		ret=MINIRPC_ENCODING_ERR;
	xdr_destroy(&xdrs);
	return ret;
}

mrpc_status_t serialize(xdrproc_t xdr_proc, void *in, char *out,
			unsigned out_len)
{
	return serialize_common(XDR_ENCODE, xdr_proc, in, out, out_len);
}

mrpc_status_t unserialize(xdrproc_t xdr_proc, char *in, unsigned in_len,
			void *out, unsigned out_len)
{
	mrpc_status_t ret;

	memset(out, 0, out_len);
	ret=serialize_common(XDR_DECODE, xdr_proc, out, in, in_len);
	if (ret) {
		/* Free partially-allocated structure */
		xdr_free(xdr_proc, out);
	}
	return ret;
}

static mrpc_status_t format_message(struct mrpc_connection *conn,
			xdrproc_t xdr_proc, void *data,
			struct mrpc_message **result)
{
	struct mrpc_message *msg;
	XDR xdrs;
	mrpc_status_t ret;

	if (xdr_proc != (xdrproc_t) xdr_void && data == NULL)
		return MINIRPC_ENCODING_ERR;
	msg=mrpc_alloc_message(conn);
	xdrlen_create(&xdrs);
	if (!xdr_proc(&xdrs, data)) {
		xdr_destroy(&xdrs);
		mrpc_free_message(msg);
		return MINIRPC_ENCODING_ERR;
	}
	msg->hdr.datalen=xdr_getpos(&xdrs);
	xdr_destroy(&xdrs);

	if (msg->hdr.datalen) {
		mrpc_alloc_message_data(msg, msg->hdr.datalen);
		ret=serialize(xdr_proc, data, msg->data, msg->hdr.datalen);
		if (ret) {
			mrpc_free_message(msg);
			return ret;
		}
	}
	*result=msg;
	return MINIRPC_OK;
}

static mrpc_status_t unformat_message(xdrproc_t type, unsigned size,
			struct mrpc_message *msg, void **result)
{
	void *buf;
	mrpc_status_t ret;

	if (size && result == NULL)
		return MINIRPC_ENCODING_ERR;
	buf=mrpc_alloc_argument(size);
	ret=unserialize(type, msg->data, msg->hdr.datalen, buf, size);
	if (ret) {
		mrpc_free_argument(NULL, buf);
		return ret;
	}
	if (result != NULL)
		*result=buf;
	return MINIRPC_OK;
}

mrpc_status_t format_request(struct mrpc_connection *conn, unsigned cmd,
			void *data, struct mrpc_message **result)
{
	struct mrpc_message *msg;
	xdrproc_t type;
	mrpc_status_t ret;

	if (conn->protocol->sender_request_info(cmd, &type, NULL))
		return MINIRPC_ENCODING_ERR;
	ret=format_message(conn, type, data, &msg);
	if (ret)
		return ret;
	msg->hdr.sequence = conn->next_sequence++;
	msg->hdr.status=MINIRPC_PENDING;
	msg->hdr.cmd=cmd;
	*result=msg;
	return MINIRPC_OK;
}

mrpc_status_t format_reply(struct mrpc_message *request, void *data,
			struct mrpc_message **result)
{
	struct mrpc_message *msg;
	xdrproc_t type;
	mrpc_status_t ret;

	if (request->conn->protocol->
				receiver_reply_info(request->hdr.cmd, &type,
				NULL))
		return MINIRPC_ENCODING_ERR;
	ret=format_message(request->conn, type, data, &msg);
	if (ret)
		return ret;
	msg->hdr.sequence=request->hdr.sequence;
	msg->hdr.status=MINIRPC_OK;
	msg->hdr.cmd=request->hdr.cmd;
	*result=msg;
	return MINIRPC_OK;
}

mrpc_status_t format_reply_error(struct mrpc_message *request,
			mrpc_status_t status, struct mrpc_message **result)
{
	struct mrpc_message *msg;

	if (status == MINIRPC_OK)
		return MINIRPC_INVALID_ARGUMENT;
	if (format_message(request->conn, (xdrproc_t)xdr_void, NULL, &msg))
		return MINIRPC_ENCODING_ERR;
	msg->hdr.sequence=request->hdr.sequence;
	msg->hdr.status=status;
	msg->hdr.cmd=request->hdr.cmd;
	*result=msg;
	return MINIRPC_OK;
}

mrpc_status_t unformat_request(struct mrpc_message *msg, void **result)
{
	xdrproc_t type;
	unsigned size;
	mrpc_status_t ret;

	if (msg->conn->protocol->receiver_request_info(msg->hdr.cmd,
				&type, &size))
		return MINIRPC_ENCODING_ERR;
	ret=unformat_message(type, size, msg, result);
	if (ret)
		g_message("Request payload deserialize failure, seq %u",
					msg->hdr.sequence);
	return ret;
}

mrpc_status_t unformat_reply(struct mrpc_message *msg, void **result)
{
	xdrproc_t type;
	unsigned size;
	mrpc_status_t ret;

	if (msg->hdr.status)
		return msg->hdr.status;
	if (msg->conn->protocol->sender_reply_info(msg->hdr.cmd,
				&type, &size))
		return MINIRPC_ENCODING_ERR;
	ret=unformat_message(type, size, msg, result);
	if (ret)
		g_message("Reply payload deserialize failure, seq %u",
					msg->hdr.sequence);
	return ret;
}
