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
struct mrpc_conn_set;
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
 * @brief Reasons that a connection could have been closed
 * @param	MRPC_DISC_USER
 *	The connection was closed with mrpc_conn_close()
 * @param	MRPC_DISC_CLOSED
 *	The connection was closed by the remote end
 * @param	MRPC_DISC_IOERR
 *	The connection was closed due to an I/O error
 */
enum mrpc_disc_reason {
	MRPC_DISC_USER,
	MRPC_DISC_CLOSED,
	MRPC_DISC_IOERR
};

/**
 * @addtogroup event
 * @{
 */

/**
 * @brief Event callback fired on connection close
 * @param	conn_data
 *	The cookie associated with the connection
 * @param	reason
 *	The reason the connection was closed
 * @sa mrpc_set_disconnect_func(), mrpc_conn_unref()
 *
 * If supplied, this callback is fired when a connection is closed for any
 * reason, including when explicitly requested by the application (with
 * mrpc_conn_close()).  Once the callback returns, the application will not
 * receive further events on this connection.  If the connection's refcount
 * is greater than zero after the disconnection function returns, the
 * connection handle will persist until all references are released.
 */
typedef void (mrpc_disconnect_fn)(void *conn_data,
			enum mrpc_disc_reason reason);

/**
 * @}
 * @addtogroup setup
 * @{
 */

/**
 * @brief Create a connection set
 * @param[out]	new_set
 *	The resulting connection set, or NULL on error
 * @param	protocol
 *	Protocol role definition for connections in this connection set
 * @param	set_data
 *	An application-specific cookie for this connection set
 * @stdreturn
 *
 * Create a connection set with a refcount of 1, associate it with the
 * specified protocol role and application-specific pointer, start its
 * background thread, and return a handle to the connection set.  If
 * @c set_data is NULL, set the application-specific pointer to the
 * connection set handle returned in @c new_set.
 */
int mrpc_conn_set_create(struct mrpc_conn_set **new_set,
			const struct mrpc_protocol *protocol, void *set_data);

/**
 * @brief Increment the refcount of a connection set
 * @param	set
 *	The connection set
 *
 * Get an additional reference to the specified connection set.
 *
 * @note Dispatcher threads should not hold their own persistent references
 * to the set for which they are dispatching.
 */
void mrpc_conn_set_ref(struct mrpc_conn_set *set);

/**
 * @brief Decrement the refcount of a connection set
 * @param	set
 *	The connection set
 *
 * Put a reference to the specified connection set.  When the refcount
 * reaches zero @em and there are no connections associated with the set,
 * the set will be destroyed.  Destruction of a connection set involves
 * the following steps:
 *
 * -# Shut down all threads started with mrpc_start_dispatch_thread(), and
 * cause all other dispatch functions to return ENXIO
 * -# Wait for dispatching threads to call mrpc_dispatcher_remove()
 * -# Shut down the background thread associated with the connection set
 * -# Free the set's data structures
 *
 * After the set's refcount reaches zero, the application must not start
 * any additional dispatchers or create any connections against the set.
 * The application should continue to dispatch events against the set
 * (if it is doing its own dispatching) until the dispatcher functions
 * return ENXIO.
 */
void mrpc_conn_set_unref(struct mrpc_conn_set *set);

/**
 * @brief Set the function to be called when a connection is closed for any
 *	reason
 * @param	set
 *	The connection set to configure
 * @param	func
 *	The disconnect function, or NULL for none
 * @stdreturn
 * @sa mrpc_disconnect_fn
 *
 * By default, no disconnect function is provided.
 */
int mrpc_set_disconnect_func(struct mrpc_conn_set *set,
			mrpc_disconnect_fn *func);

/**
 * @}
 * @addtogroup conn
 * @{
 */

/**
 * @brief Create a new connection handle
 * @param[out]	new_conn
 *	The resulting connection handle, or NULL on error
 * @param	set
 *	The set to associate with this connection
 * @param	data
 *	An application-specific cookie for this connection
 * @stdreturn
 *
 * Allocate a new connection handle with a refcount of 1, and associate it
 * with the given connection set and application-specific pointer.  This
 * handle can then be bound to an existing socket with mrpc_bind_fd().
 * Before the connection is completed, the only valid operations on the
 * connection handle are:
 * - Set the operations structure using the set_operations function for this
 * protocol role
 * - Update its refcount with mrpc_conn_ref() / mrpc_conn_unref().  If the last
 * reference is removed with mrpc_conn_unref(), the handle is freed.
 *
 * If @c data is NULL, the application-specific pointer is set to the
 * connection handle returned in @c new_conn.
 *
 * While the connection handle exists, it holds a reference on the associated
 * connection set.
 */
int mrpc_conn_create(struct mrpc_connection **new_conn,
			struct mrpc_conn_set *set, void *data);

/**
 * @brief Increment the refcount of a connection
 * @param	conn
 *	The connection
 *
 * Get an additional reference to the specified connection.
 */
void mrpc_conn_ref(struct mrpc_connection *conn);

/**
 * @brief Decrement the refcount of a connection
 * @param	conn
 *	The connection
 *
 * Put a reference to the specified connection.  When the refcount reaches
 * zero @em and the connection is no longer active, the connection will be
 * destroyed.  Connections become active when they are connected using
 * mrpc_bind_fd(), or when miniRPC passes them to the connection set's
 * accept function.  Active connections become inactive after they are
 * closed @em and the disconnect function returns.
 */
void mrpc_conn_unref(struct mrpc_connection *conn);

/**
 * @brief Bind an existing file descriptor to a connection handle
 * @param	conn
 *	The connection handle
 * @param	fd
 *	The file descriptor to bind
 * @stdreturn
 *
 * Associate the specified socket with an existing miniRPC connection handle.
 * The handle must not have been connected already.  The handle may have
 * either a client or server role.  The connection set's accept function
 * will @em not be called.  To avoid races, the application should ensure
 * that the operations structure is set on the connection handle, if
 * necessary, @em before calling this function.
 *
 * The specified file descriptor must be associated with a connected socket
 * of type SOCK_STREAM.  After this call, the socket will be managed by
 * miniRPC; the application must not read, write, or close it directly.
 */
int mrpc_bind_fd(struct mrpc_connection *conn, int fd);

/**
 * @brief Close an existing connection
 * @param	conn
 *	The connection to close
 * @return 0 on success, EALREADY if mrpc_conn_close() has already been called
 *	on this connection, or ENOTCONN if the connection handle has never
 *	been connected
 *
 * Close the connection specified by @c conn.  Protocol messages already
 * queued for transmission will be sent before the socket is closed.
 * Any pending synchronous RPCs will return ::MINIRPC_NETWORK_FAILURE,
 * and asynchronous RPCs will have their callbacks fired with a status
 * code of ::MINIRPC_NETWORK_FAILURE.  Other events queued for the
 * application will be dropped.
 *
 * There is a window of time after this function returns in which further
 * non-error events may occur on the connection.  The application must be
 * prepared to handle these events.  If this function is called from an
 * event handler for the connection being closed, and the handler has not
 * called mrpc_release_event(), then the application is guaranteed that no
 * more non-error events will occur on the connection once the call returns.
 *
 * The application must not free any supporting data structures until the
 * connection set's disconnect function is called for the connection, since
 * further events may be pending.  In addition, the application should not
 * assume that the disconnect function's @c reason argument will be
 * ::MRPC_DISC_USER, since the connection may have been terminated for
 * another reason before mrpc_conn_close() was called.
 *
 * This function may be called from an event handler, including an event
 * handler for the connection being closed.
 */
int mrpc_conn_close(struct mrpc_connection *conn);


/**
 * @}
 * @addtogroup event
 * @{
 */

/**
 * @brief Start a dispatcher thread for a connection set
 * @param	set
 *	The connection set
 * @stdreturn
 *
 * Start a background thread to dispatch events.  This thread will persist
 * until the connection set is destroyed, at which point it will exit.  This
 * function can be called more than once; each call will create a new thread.
 * This is the simplest way to start a dispatcher for a connection set.
 *
 * Unlike with mrpc_dispatch() and mrpc_dispatch_loop(), the caller does not
 * need to register the dispatcher thread with mrpc_dispatcher_add().  The
 * background thread handles this for you.
 */
int mrpc_start_dispatch_thread(struct mrpc_conn_set *set);

/**
 * @brief Notify miniRPC that the current thread will dispatch events for this
 *	connection set
 * @param	set
 *	The connection set
 *
 * Any thread which calls mrpc_dispatch() or mrpc_dispatch_loop() must call
 * mrpc_dispatcher_add() before it starts dispatching for the specified
 * connection set.
 */
void mrpc_dispatcher_add(struct mrpc_conn_set *set);

/**
 * @brief Notify miniRPC that the current thread will no longer dispatch
 *	events for this connection set
 * @param	set
 *	The connection set
 *
 * Any thread which calls mrpc_dispatch() or mrpc_dispatch_loop() must call
 * mrpc_dispatcher_remove() when it decides it will no longer dispatch for
 * the specified connection set.
 */
void mrpc_dispatcher_remove(struct mrpc_conn_set *set);

/**
 * @brief Dispatch events from this thread until the connection set is
 *	destroyed
 * @param	set
 *	The connection set
 * @return ENXIO if the connection set is being destroyed, or a POSIX
 *	error code on other error
 *
 * Start dispatching events for the given connection set, and do not return
 * until the connection set is being destroyed.  The thread must call
 * mrpc_dispatcher_add() before calling this function, and
 * mrpc_dispatcher_remove() afterward.  This function must not be called
 * from an event handler.
 */
int mrpc_dispatch_loop(struct mrpc_conn_set *set);

/**
 * @brief Dispatch events from this thread and then return
 * @param	set
 *	The connection set
 * @param	max
 *	The maximum number of events to dispatch, or 0 for no limit
 * @sa mrpc_get_event_fd()
 * @return ENXIO if the connection set is being destroyed, 0 if more events
 *	are pending, or EAGAIN if the event queue is empty
 *
 * Dispatch events until there are no more events to process or until
 * @c max events have been processed, whichever comes first; if @c max is 0,
 * dispatch until there are no more events to process.  The calling thread
 * must call mrpc_dispatcher_add() before calling this function for the first
 * time.
 *
 * If this function returns ENXIO, the connection set is being destroyed.
 * The application must stop calling this function, and must call
 * mrpc_dispatcher_remove() to indicate its intent to do so.
 *
 * This function must not be called from an event handler.
 */
int mrpc_dispatch(struct mrpc_conn_set *set, int max);

/**
 * @brief Obtain a file descriptor which will be readable when there are
 *	events to process
 * @param	set
 *	The connection set
 * @return The file descriptor
 *
 * Returns a file descriptor which can be passed to select()/poll() to
 * determine when the connection set has events to process.  This can be
 * used to embed processing of miniRPC events into an application-specific
 * event loop.  When the descriptor is readable, the connection set has
 * events to be dispatched; the application can call mrpc_dispatch() to
 * handle them.
 *
 * The application must not read, write, or close the provided file
 * descriptor.  Once mrpc_dispatch() returns ENXIO, indicating that the
 * connection set is being shut down, the application must stop polling
 * on the descriptor.
 */
int mrpc_get_event_fd(struct mrpc_conn_set *set);

/**
 * @}
 * @addtogroup error
 * @{
 */

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

/**
 * @}
 */

#endif
