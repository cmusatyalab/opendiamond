/*
 * miniRPC - TCP RPC library with asynchronous operations
 *
 * Copyright (C) 2007-2008 Carnegie Mellon University
 *
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU Lesser General Public License version 2.1, as shown in
 * the file COPYING.
 */

#ifndef MINIRPC_INTERNAL
#error This header is for internal use by the miniRPC protocol library
#endif

#ifndef MINIRPC_INTERNAL_H
#define MINIRPC_INTERNAL_H
#define MINIRPC_PROTOCOL

#define G_LOG_DOMAIN "minirpc"

#include <sys/types.h>
#include <sys/socket.h>
#include <glib.h>
#include <minirpc/minirpc.h>
#include <minirpc/protocol.h>
#include "minirpc_xdr.h"

#define exported __attribute__ ((visibility ("default")))

struct mrpc_message {
	struct mrpc_connection *conn;
	struct mrpc_header hdr;
	char *data;
};

/* No longer an exported status code; now just an implementation detail
   to indicate that a message is a request */
#define MINIRPC_PENDING -1

struct mrpc_connection {
	const struct mrpc_protocol *protocol;
	const void *operations;
	int fd;
	void *private;
	int next_sequence;
};

/* connection.c */
mrpc_status_t send_message(struct mrpc_message *msg);
mrpc_status_t receive_message(struct mrpc_connection *conn,
			struct mrpc_message **_msg);

/* message.c */
struct mrpc_message *mrpc_alloc_message(struct mrpc_connection *conn);
void mrpc_free_message(struct mrpc_message *msg);
void mrpc_alloc_message_data(struct mrpc_message *msg, unsigned len);

/* serialize.c */
void *mrpc_alloc_argument(unsigned len);
void mrpc_free_argument(xdrproc_t xdr_proc, void *buf);
mrpc_status_t serialize(xdrproc_t xdr_proc, void *in, char *out,
			unsigned out_len);
mrpc_status_t unserialize(xdrproc_t xdr_proc, char *in, unsigned in_len,
			void *out, unsigned out_len);
mrpc_status_t format_request(struct mrpc_connection *conn, unsigned cmd,
			void *data, struct mrpc_message **result);
mrpc_status_t format_reply(struct mrpc_message *request, void *data,
			struct mrpc_message **result);
mrpc_status_t format_reply_error(struct mrpc_message *request,
			mrpc_status_t status, struct mrpc_message **result);
mrpc_status_t unformat_request(struct mrpc_message *msg, void **result);
mrpc_status_t unformat_reply(struct mrpc_message *msg, void **result);

/* util.c */
int set_blocking(int fd);
int set_cloexec(int fd);

/* xdr_len.c */
void xdrlen_create(XDR *xdrs);

#endif
