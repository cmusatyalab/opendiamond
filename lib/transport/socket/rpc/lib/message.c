/*
 * miniRPC - Simple TCP RPC library
 *
 * Copyright (C) 2007-2010 Carnegie Mellon University
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#include <assert.h>
#include <errno.h>
#define MINIRPC_INTERNAL
#include "internal.h"

void mrpc_alloc_message_data(struct mrpc_message *msg, unsigned len)
{
	assert(msg->data == NULL);
	msg->data=g_malloc(len);
}

static void mrpc_free_message_data(struct mrpc_message *msg)
{
	if (msg->data) {
		g_free(msg->data);
		msg->data=NULL;
	}
}

struct mrpc_message *mrpc_alloc_message(struct mrpc_connection *conn)
{
	struct mrpc_message *msg;

	msg=g_slice_new0(struct mrpc_message);
	msg->conn=conn;
	return msg;
}

void mrpc_free_message(struct mrpc_message *msg)
{
	mrpc_free_message_data(msg);
	g_slice_free(struct mrpc_message, msg);
}

static mrpc_status_t mrpc_send_reply(const struct mrpc_protocol *protocol,
			int cmd, struct mrpc_message *request, void *data)
{
	struct mrpc_message *reply;
	mrpc_status_t ret;

	if (request == NULL || cmd != request->hdr.cmd)
		return MINIRPC_INVALID_ARGUMENT;
	if (protocol != request->conn->protocol)
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

static mrpc_status_t mrpc_send_reply_error(
			const struct mrpc_protocol *protocol,
			int cmd, struct mrpc_message *request,
			mrpc_status_t status)
{
	struct mrpc_message *reply;
	mrpc_status_t ret;

	if (request == NULL || cmd != request->hdr.cmd ||
				status == MINIRPC_OK ||
				status == MINIRPC_PENDING)
		return MINIRPC_INVALID_ARGUMENT;
	if (protocol != request->conn->protocol)
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

static void fail_request(struct mrpc_message *request, mrpc_status_t err)
{
	if (mrpc_send_reply_error(request->conn->protocol,
				request->hdr.cmd, request, err))
		mrpc_free_message(request);
}

static void dispatch_request(struct mrpc_message *request)
{
	struct mrpc_connection *conn=request->conn;
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
		fail_request(request, MINIRPC_PROCEDURE_UNAVAIL);
		return;
	}

	if (conn->protocol->receiver_reply_info(request->hdr.cmd,
				&reply_type, &reply_size)) {
		/* Can't happen if the info tables are well-formed */
		fail_request(request, MINIRPC_ENCODING_ERR);
		return;
	}
	reply_data=mrpc_alloc_argument(reply_size);
	ret=unformat_request(request, &request_data);
	if (ret) {
		/* Invalid datalen, etc. */
		fail_request(request, ret);
		mrpc_free_argument(NULL, reply_data);
		return;
	}
	/* We don't need the serialized request data anymore.  The request
	   struct may stay around for a while, so free up some memory. */
	mrpc_free_message_data(request);

	assert(conn->protocol->request != NULL);
	result=conn->protocol->request(conn->operations, conn->private,
				request->hdr.cmd, request_data, reply_data);
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
		if (ret != MINIRPC_NETWORK_FAILURE)
			g_message("Reply failed, seq %u cmd %d status %d "
					"err %d", request->hdr.sequence,
					request->hdr.cmd, result, ret);
		mrpc_free_message(request);
	}
}

exported void mrpc_dispatch_loop(struct mrpc_connection *conn)
{
	struct mrpc_message *msg;
	mrpc_status_t ret = MINIRPC_OK;

	if (conn == NULL)
		return;
	while (ret != MINIRPC_NETWORK_FAILURE) {
		ret = receive_message(conn, &msg);
		if (ret)
			continue;
		if (msg->hdr.status == MINIRPC_PENDING) {
			dispatch_request(msg);
		} else {
			g_message("Unexpected reply, seq %u cmd %d "
					"status %d len %u",
					msg->hdr.sequence, msg->hdr.cmd,
					msg->hdr.status, msg->hdr.datalen);
			mrpc_free_message(msg);
		}
	}
}

static gboolean reply_header_ok(struct mrpc_header *req_hdr,
			struct mrpc_message *msg)
{
	if (req_hdr->sequence != msg->hdr.sequence) {
		g_message("Mismatched sequence in reply, expected %d, "
					"found %d", req_hdr->sequence,
					msg->hdr.sequence);
		return FALSE;
	} else if (req_hdr->cmd != msg->hdr.cmd) {
		g_message("Mismatched command field in reply, "
					"seq %u, expected cmd %d, found %d",
					msg->hdr.sequence, req_hdr->cmd,
					msg->hdr.cmd);
		return FALSE;
	} else if (msg->hdr.status != 0 && msg->hdr.datalen != 0) {
		g_message("Reply with both error and payload, seq %u",
					msg->hdr.sequence);
		return FALSE;
	}
	return TRUE;
}

exported mrpc_status_t mrpc_send_request(const struct mrpc_protocol *protocol,
			struct mrpc_connection *conn, int cmd, void *in,
			void **out)
{
	struct mrpc_message *msg;
	struct mrpc_header req_hdr;
	mrpc_status_t ret;

	if (out != NULL)
		*out=NULL;
	if (conn == NULL || cmd <= 0)
		return MINIRPC_INVALID_ARGUMENT;
	if (protocol != conn->protocol)
		return MINIRPC_INVALID_PROTOCOL;
	ret=format_request(conn, cmd, in, &msg);
	if (ret)
		return ret;
	req_hdr = msg->hdr;
	ret=send_message(msg);
	if (ret)
		return ret;

	while (ret != MINIRPC_NETWORK_FAILURE) {
		ret = receive_message(conn, &msg);
		if (ret)
			continue;
		if (msg->hdr.status == MINIRPC_PENDING) {
			dispatch_request(msg);
		} else if (!reply_header_ok(&req_hdr, msg)) {
			mrpc_free_message(msg);
			return MINIRPC_ENCODING_ERR;
		} else {
			ret = unformat_reply(msg, out);
			mrpc_free_message(msg);
			return ret;
		}
	}
	return ret;
}
