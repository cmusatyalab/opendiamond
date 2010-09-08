/*
 * miniRPC - Simple TCP RPC library
 *
 * Copyright (C) 2007-2008 Carnegie Mellon University
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
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
	mrpc_status_t (*request)(const void *ops, void *conn_data,
				int cmd, void *in, void *out);
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

#endif
