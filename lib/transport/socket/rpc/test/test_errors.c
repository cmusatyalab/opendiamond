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
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "common.h"

mrpc_status_t do_ping(void *conn_data, struct mrpc_message *msg)
{
	expect(proto_client_check_int_send_async_reply(msg),
				MINIRPC_INVALID_PROTOCOL);
	expect(proto_client_check_int_send_async_reply_error(msg, 1),
				MINIRPC_INVALID_PROTOCOL);
	expect(proto_ping_send_async_reply_error(msg, 0),
				MINIRPC_INVALID_ARGUMENT);
	expect(proto_check_int_send_async_reply(msg),
				MINIRPC_INVALID_ARGUMENT);
	expect(proto_check_int_send_async_reply_error(msg, 1),
				MINIRPC_INVALID_ARGUMENT);
	expect(proto_ping_send_async_reply(msg), 0);
	return MINIRPC_PENDING;
}

const struct proto_server_operations probe_ops = {
	.ping = do_ping
};

void *probe_server_accept(void *set_data, struct mrpc_connection *conn,
			struct sockaddr *from, socklen_t from_len)
{
	if (proto_server_set_operations(conn, &probe_ops))
		die("Error setting operations struct");
	return conn;
}

int main(int argc, char **argv)
{
	struct mrpc_conn_set *sset;
	struct mrpc_conn_set *cset;
	struct mrpc_connection *conn;
	struct sockaddr_in addr;
	char *port;
	int fd;
	int fdpair[2];
	IntParam ip = {INT_VALUE};
	IntParam *ipp;

	expect(mrpc_conn_set_create(NULL, proto_server, NULL), EINVAL);
	sset=(void*)1;
	expect(mrpc_conn_set_create(&sset, NULL, NULL), EINVAL);
	expect(sset == NULL, 1);
	mrpc_conn_set_ref(NULL);
	mrpc_conn_set_unref(NULL);
	mrpc_conn_ref(NULL);
	mrpc_conn_unref(NULL);
	expect(mrpc_start_dispatch_thread(NULL), EINVAL);
	mrpc_dispatcher_add(NULL);
	mrpc_dispatcher_remove(NULL);
	expect(mrpc_dispatch_loop(NULL), EINVAL);
	expect(mrpc_dispatch(NULL, 0), EINVAL);
	expect(mrpc_strerror(MINIRPC_PENDING) != NULL, 1);
	expect(strcmp(mrpc_strerror(-12), "Unknown error"), 0);
	expect(strcmp(mrpc_strerror(12), "Application-specific error"), 0);

	if (mrpc_conn_set_create(&sset, proto_server, NULL))
		die("Couldn't allocate conn set");
	if (mrpc_conn_set_create(&cset, proto_client, NULL))
		die("Couldn't allocate conn set");
	expect(mrpc_set_accept_func(sset, NULL), EINVAL);
	expect(mrpc_set_accept_func(cset, sync_server_accept), EINVAL);

	if (mrpc_set_accept_func(sset, sync_server_accept))
		die("Couldn't set accept func");
	if (mrpc_set_disconnect_func(sset, disconnect_normal))
		die("Couldn't set disconnect func");
	if (mrpc_set_disconnect_func(cset, disconnect_user))
		die("Couldn't set disconnect func");
	start_monitored_dispatcher(sset);
	start_monitored_dispatcher(cset);

	expect(mrpc_dispatch(cset, 1), EPERM);
	expect(mrpc_dispatch_loop(cset), EPERM);

	port=NULL;
	expect(mrpc_listen(NULL, AF_UNSPEC, "localhost", &port), EINVAL);
	expect(mrpc_listen(sset, AF_UNSPEC, "localhost", NULL), EINVAL);
	port=NULL;
	expect(mrpc_listen(sset, AF_UNSPEC, "localhost", &port), EINVAL);
	port="50234";
	expect(mrpc_listen(sset, 90500, "localhost", &port), EAFNOSUPPORT);
	port=NULL;
	expect(mrpc_listen(sset, AF_INET, NULL, &port), 0);
	expect(mrpc_conn_create(&conn, cset, NULL), 0);
	expect(mrpc_connect(conn, AF_UNSPEC, NULL, port), 0);
	expect(proto_ping(conn), 0);
	mrpc_listen_close(NULL);
	mrpc_listen_close(sset);
	mrpc_conn_unref(conn);
	expect(mrpc_conn_close(conn), 0);
	expect(mrpc_conn_close(NULL), EINVAL);
	expect(mrpc_conn_create(&conn, cset, NULL), 0);
	expect(mrpc_connect(conn, AF_UNSPEC, NULL, port), ECONNREFUSED);
	expect(mrpc_conn_close(conn), ENOTCONN);
	mrpc_conn_unref(conn);
	free(port);

	port=NULL;
	expect(mrpc_listen(sset, AF_INET, "localhost", &port), 0);
	expect(mrpc_conn_create(NULL, cset, NULL), EINVAL);
	conn=(void*)1;
	expect(mrpc_conn_create(&conn, NULL, NULL), EINVAL);
	expect(conn == NULL, 1);
	expect(mrpc_conn_create(&conn, cset, NULL), 0);
	expect(mrpc_connect(NULL, AF_INET, "localhost", port), EINVAL);
	expect(mrpc_connect(conn, 90500, "localhost", port), EAFNOSUPPORT);
	expect(mrpc_connect(conn, AF_INET, "localhost", NULL), EINVAL);

	fd=socket(PF_INET, SOCK_STREAM, 0);
	if (fd == -1)
		die("Couldn't create socket");
	addr.sin_family=AF_INET;
	addr.sin_port=htons(atoi(port));
	addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
	expect(connect(fd, (struct sockaddr *)&addr, sizeof(addr)), 0);
	expect(mrpc_bind_fd(NULL, fd), EINVAL);
	expect(mrpc_bind_fd(conn, fd), 0);
	expect(mrpc_connect(conn, AF_INET, NULL, port), EINVAL);
	expect(mrpc_bind_fd(conn, fd), EINVAL);
	expect(mrpc_conn_close(conn), 0);
	mrpc_conn_unref(conn);
	expect(mrpc_conn_create(&conn, cset, NULL), 0);
	fd=socket(PF_INET, SOCK_STREAM, 0);
	if (fd == -1)
		die("Couldn't create socket");
	expect(mrpc_bind_fd(conn, fd), ENOTCONN);
	expect(listen(fd, 16), 0);
	expect(mrpc_bind_fd(conn, fd), ENOTCONN);
	close(fd);
	fd=open("/dev/null", O_RDWR);
	if (fd == -1)
		die("Couldn't open /dev/null");
	expect(mrpc_bind_fd(conn, fd), ENOTSOCK);
	close(fd);
	expect(socketpair(AF_UNIX, SOCK_DGRAM, 0, fdpair), 0);
	expect(mrpc_bind_fd(conn, fdpair[0]), EPROTONOSUPPORT);
	close(fdpair[0]);
	close(fdpair[1]);

	expect(mrpc_connect(conn, AF_UNSPEC, NULL, port), 0);
	expect(proto_client_set_operations(NULL, NULL), EINVAL);
	expect(proto_ping(NULL), MINIRPC_INVALID_ARGUMENT);
	expect(proto_ping_async(conn, NULL, NULL), MINIRPC_INVALID_ARGUMENT);
	expect(proto_notify(NULL, NULL), MINIRPC_INVALID_ARGUMENT);
	ipp=(void*)1;
	expect(proto_loop_int(conn, NULL, &ipp), MINIRPC_ENCODING_ERR);
	expect(ipp == NULL, 1);
	ipp=(void*)1;
	expect(proto_loop_int(NULL, NULL, &ipp), MINIRPC_INVALID_ARGUMENT);
	expect(ipp == NULL, 1);
	expect(proto_loop_int(conn, &ip, NULL), MINIRPC_ENCODING_ERR);
	expect(proto_ping_send_async_reply(NULL), MINIRPC_INVALID_ARGUMENT);
	expect(proto_ping_send_async_reply_error(NULL, 0),
				MINIRPC_INVALID_ARGUMENT);
	expect(proto_client_check_int(conn, NULL), MINIRPC_INVALID_PROTOCOL);
	expect(proto_client_check_int_async(conn,
				(proto_client_check_int_callback_fn*)1,
				NULL, NULL), MINIRPC_INVALID_PROTOCOL);
	expect(mrpc_conn_close(conn), 0);
	mrpc_conn_unref(conn);

	expect(mrpc_conn_create(&conn, cset, NULL), 0);
	expect(proto_ping(conn), MINIRPC_INVALID_ARGUMENT);
	expect(mrpc_conn_close(conn), ENOTCONN);
	mrpc_conn_unref(conn);
	mrpc_conn_set_unref(cset);
	mrpc_listen_close(sset);
	mrpc_conn_set_unref(sset);
	free(port);

	sset=spawn_server(&port, proto_server, probe_server_accept, NULL, 1);
	if (mrpc_set_disconnect_func(sset, disconnect_normal))
		die("Couldn't set disconnect func");
	if (mrpc_conn_set_create(&cset, proto_client, NULL))
		die("Couldn't allocate conn set");
	if (mrpc_set_disconnect_func(cset, disconnect_user))
		die("Couldn't set disconnect func");
	start_monitored_dispatcher(cset);
	expect(mrpc_conn_create(&conn, cset, NULL), 0);
	expect(mrpc_connect(conn, AF_INET, NULL, port), 0);
	expect(proto_ping(conn), 0);
	expect(mrpc_conn_close(conn), 0);
	mrpc_conn_unref(conn);
	mrpc_conn_set_unref(cset);
	mrpc_listen_close(sset);
	mrpc_conn_set_unref(sset);
	free(port);

	free_IntArray(NULL, 0);
	free_IntArray(NULL, 1);

	return 0;
}
