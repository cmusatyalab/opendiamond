/*
 * miniRPC - TCP RPC library with asynchronous operations
 *
 * Copyright (C) 2007-2008 Carnegie Mellon University
 *
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU Lesser General Public License version 2.1, as shown in
 * the file COPYING.
 */

#ifndef MINIRPC_PROTOCOL
#error This header is for use by miniRPC stub code only
#endif

#ifndef MINIRPC_PROTOCOL_H
#define MINIRPC_PROTOCOL_H

#include <rpc/types.h>
#include <rpc/xdr.h>
#include <minirpc/minirpc.h>

#define SET_PTR_IF_NOT_NULL(ptr, val) do { \
		if ((ptr) != NULL) \
			*(ptr)=(val); \
	} while (0)

struct mrpc_protocol {
	int is_server;
	mrpc_status_t (*request)(const void *ops, void *conn_data,
				struct mrpc_message *msg, int cmd, void *in,
				void *out);
	mrpc_status_t (*sender_request_info)(unsigned cmd, xdrproc_t *type,
				unsigned *size);
	mrpc_status_t (*sender_reply_info)(unsigned cmd, xdrproc_t *type,
				unsigned *size);
	mrpc_status_t (*receiver_request_info)(unsigned cmd, xdrproc_t *type,
				unsigned *size);
	mrpc_status_t (*receiver_reply_info)(unsigned cmd, xdrproc_t *type,
				unsigned *size);
};

/* connection.c */
int mrpc_conn_set_operations(struct mrpc_connection *conn,
			const struct mrpc_protocol *protocol, const void *ops);

/* message.c */
mrpc_status_t mrpc_send_request(const struct mrpc_protocol *protocol,
			struct mrpc_connection *conn, int cmd, void *in,
			void **out);
mrpc_status_t mrpc_send_request_noreply(const struct mrpc_protocol *protocol,
			struct mrpc_connection *conn, int cmd, void *in);

#endif
