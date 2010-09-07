/*
 * miniRPC - TCP RPC library with asynchronous operations
 *
 * Copyright (C) 2007-2008 Carnegie Mellon University
 *
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU Lesser General Public License version 2.1, as shown in
 * the file COPYING.
 */

/**
 * @file
 * @brief Common interface to the miniRPC library
 */

#ifndef MINIRPC_H
#define MINIRPC_H

#include <sys/types.h>
#include <sys/socket.h>
#include <stdint.h>

struct mrpc_protocol;
struct mrpc_connection;

/**
 * @brief Error codes defined by miniRPC
 * @ingroup error
 * @param	MINIRPC_OK
 *	Success
 * @param	MINIRPC_ENCODING_ERR
 *	An error occurred during serialization/deserialization
 * @param	MINIRPC_PROCEDURE_UNAVAIL
 *	The requested procedure is not available at this time
 * @param	MINIRPC_INVALID_ARGUMENT
 *	An invalid argument was provided
 * @param	MINIRPC_INVALID_PROTOCOL
 *	The implied protocol role does not match the connection
 * @param	MINIRPC_NETWORK_FAILURE
 *	The action could not be completed due to a temporary or permanent
 *	network problem
 */
enum mrpc_status_codes {
	MINIRPC_OK			=  0,
	MINIRPC_ENCODING_ERR		= -2,
	MINIRPC_PROCEDURE_UNAVAIL	= -3,
	MINIRPC_INVALID_ARGUMENT	= -4,
	MINIRPC_INVALID_PROTOCOL	= -5,
	MINIRPC_NETWORK_FAILURE		= -6,
};

/**
 * @brief Error code returned by protocol operations
 * @ingroup error
 */
typedef int mrpc_status_t;

/**
 * @brief Create a new connection handle
 * @param[out]	new_conn
 *	The resulting connection handle, or NULL on error
 * @param	protocol
 *	Protocol role definition for this connection
 * @param	fd
 *	The file descriptor to bind
 * @param	data
 *	An application-specific cookie for this connection
 * @stdreturn
 *
 * Allocate a new connection handle and associate it with the given
 * protocol role, socket file descriptor, and application-specific pointer.
 * The specified file descriptor must be associated with a connected socket
 * of type SOCK_STREAM.  After this call, the socket will be managed by
 * miniRPC; the application must not read, write, or close it directly.
 *
 * If @c data is NULL, the application-specific pointer is set to the
 * connection handle returned in @c new_conn.
 */
int mrpc_conn_create(struct mrpc_connection **new_conn,
			const struct mrpc_protocol *protocol, int fd,
			void *data);

/**
 * @brief Close an existing connection
 * @param	conn
 *	The connection to close
 * @return 0 on success or a POSIX error code on error
 *
 * Close the connection specified by @c conn.  Future RPCs will fail with
 * ::MINIRPC_NETWORK_FAILURE and existing dispatch loops will return with
 * the same error code.
 *
 * This function may be called from an event handler, including an event
 * handler for the connection being closed.
 */
int mrpc_conn_close(struct mrpc_connection *conn);

/**
 * @brief Destroy a connection
 * @param	conn
 *	The connection
 *
 * Destroy the specified connection.  If it is still open, it will be
 * closed.
 */
void mrpc_conn_free(struct mrpc_connection *conn);

/**
 * @brief Dispatch events from this thread until the connection is destroyed
 * @param	conn
 *	The connection
 *
 * Start dispatching events for the given connection, and do not return
 * until the connection is closed.
 */
void mrpc_dispatch_loop(struct mrpc_connection *conn);

/**
 * @brief Return a brief description of the specified miniRPC error code
 * @param	status
 *	The error code
 * @return A description of the error
 *
 * The returned string must not be modified or freed by the application.
 * This function only understands the error codes defined in
 * enum ::mrpc_status_codes; application-specific error codes will be
 * mapped to a generic description.
 */
const char *mrpc_strerror(mrpc_status_t status);

#endif
