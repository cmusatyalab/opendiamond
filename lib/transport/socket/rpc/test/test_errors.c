/*
 * miniRPC - TCP RPC library with asynchronous operations
 *
 * Copyright (C) 2007-2010 Carnegie Mellon University
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

int main(int argc, char **argv)
{
	struct mrpc_conn_set *sset;
	struct mrpc_conn_set *cset;
	struct mrpc_connection *sconn;
	struct mrpc_connection *conn;
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
	expect(mrpc_strerror(-1) != NULL, 1);
	expect(strcmp(mrpc_strerror(-12), "Unknown error"), 0);
	expect(strcmp(mrpc_strerror(12), "Application-specific error"), 0);

	if (mrpc_conn_set_create(&sset, proto_server, NULL))
		die("Couldn't allocate conn set");
	if (mrpc_conn_set_create(&cset, proto_client, NULL))
		die("Couldn't allocate conn set");

	if (mrpc_set_disconnect_func(sset, disconnect_normal))
		die("Couldn't set disconnect func");
	if (mrpc_set_disconnect_func(cset, disconnect_user))
		die("Couldn't set disconnect func");
	start_monitored_dispatcher(sset);
	start_monitored_dispatcher(cset);

	expect(mrpc_dispatch(cset, 1), EPERM);
	expect(mrpc_dispatch_loop(cset), EPERM);

	expect(mrpc_conn_create(&sconn, sset, NULL), 0);
	sync_server_set_ops(sconn);
	expect(mrpc_conn_create(&conn, cset, NULL), 0);
	bind_conn_pair(sconn, conn);
	mrpc_conn_unref(conn);
	expect(mrpc_conn_close(conn), 0);
	expect(mrpc_conn_close(NULL), EINVAL);
	expect(mrpc_conn_create(&conn, cset, NULL), 0);
	expect(mrpc_conn_close(conn), ENOTCONN);
	mrpc_conn_unref(conn);

	expect(mrpc_conn_create(NULL, cset, NULL), EINVAL);
	conn=(void*)1;
	expect(mrpc_conn_create(&conn, NULL, NULL), EINVAL);
	expect(conn == NULL, 1);
	expect(mrpc_conn_create(&conn, cset, NULL), 0);

	get_conn_pair(&fdpair[0], &fdpair[1]);
	expect(mrpc_bind_fd(NULL, fdpair[0]), EINVAL);
	expect(mrpc_bind_fd(conn, fdpair[0]), 0);
	expect(mrpc_bind_fd(conn, fdpair[0]), EINVAL);
	expect(mrpc_conn_close(conn), 0);
	mrpc_conn_unref(conn);
	expect(close(fdpair[1]), 0);
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

	expect(mrpc_conn_create(&sconn, sset, NULL), 0);
	sync_server_set_ops(sconn);
	bind_conn_pair(conn, sconn);
	expect(proto_client_set_operations(NULL, NULL), EINVAL);
	expect(proto_ping(NULL), MINIRPC_INVALID_ARGUMENT);
	ipp=(void*)1;
	expect(proto_loop_int(conn, NULL, &ipp), MINIRPC_ENCODING_ERR);
	expect(ipp == NULL, 1);
	ipp=(void*)1;
	expect(proto_loop_int(NULL, NULL, &ipp), MINIRPC_INVALID_ARGUMENT);
	expect(ipp == NULL, 1);
	expect(proto_loop_int(conn, &ip, NULL), MINIRPC_ENCODING_ERR);
	expect(proto_client_check_int(conn, NULL), MINIRPC_INVALID_PROTOCOL);
	expect(mrpc_conn_close(conn), 0);
	expect(proto_ping(conn), MINIRPC_NETWORK_FAILURE);
	expect(mrpc_conn_close(conn), EALREADY);
	mrpc_conn_unref(conn);

	expect(mrpc_conn_create(&conn, cset, NULL), 0);
	expect(proto_ping(conn), MINIRPC_INVALID_ARGUMENT);
	expect(mrpc_conn_close(conn), ENOTCONN);
	mrpc_conn_unref(conn);
	mrpc_conn_set_unref(cset);
	mrpc_conn_set_unref(sset);

	free_IntArray(NULL, 0);
	free_IntArray(NULL, 1);

	return 0;
}
