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
#include <glib.h>
#include "common.h"

static void swallow_g_log(const gchar *domain, GLogLevelFlags level,
			const gchar *message, void *data)
{
	return;
}

int main(int argc, char **argv)
{
	struct mrpc_connection *sconn;
	struct mrpc_connection *conn;
	int fd;
	int fdpair[2];
	IntParam ip = {INT_VALUE};
	IntParam *ipp;

	g_thread_init(NULL);
	g_log_set_handler("minirpc", G_LOG_LEVEL_MESSAGE, swallow_g_log, NULL);

	mrpc_dispatch_loop(NULL);
	expect(mrpc_strerror(-1) != NULL, 1);
	expect(strcmp(mrpc_strerror(-12), "Unknown error"), 0);
	expect(strcmp(mrpc_strerror(12), "Application-specific error"), 0);

	get_conn_pair(&fdpair[0], &fdpair[1]);
	expect(mrpc_conn_create(NULL, proto_server, fdpair[0], NULL), EINVAL);
	sconn=(void*)1;
	expect(mrpc_conn_create(&sconn, NULL, fdpair[0], NULL), EINVAL);
	expect(sconn == NULL, 1);
	expect(mrpc_conn_create(&sconn, proto_server, -1, NULL), EBADF);
	expect(mrpc_conn_close(NULL), EINVAL);
	expect(close(fdpair[0]), 0);
	expect(close(fdpair[1]), 0);

	fd=socket(PF_INET, SOCK_STREAM, 0);
	if (fd == -1)
		die("Couldn't create socket");
	expect(mrpc_conn_create(&conn, proto_client, fd, NULL), ENOTCONN);
	expect(listen(fd, 16), 0);
	expect(mrpc_conn_create(&conn, proto_client, fd, NULL), ENOTCONN);
	close(fd);
	fd=open("/dev/null", O_RDWR);
	if (fd == -1)
		die("Couldn't open /dev/null");
	expect(mrpc_conn_create(&conn, proto_client, fd, NULL), ENOTSOCK);
	close(fd);
	expect(socketpair(AF_UNIX, SOCK_DGRAM, 0, fdpair), 0);
	expect(mrpc_conn_create(&conn, proto_client, fd, NULL),
				EPROTONOSUPPORT);
	close(fdpair[0]);
	close(fdpair[1]);

	get_conn_pair(&fdpair[0], &fdpair[1]);
	expect(mrpc_conn_create(&sconn, proto_server, fdpair[0], NULL), 0);
	sync_server_set_ops(sconn);
	start_monitored_dispatcher(sconn);
	expect(mrpc_conn_create(&conn, proto_client, fdpair[1], NULL), 0);
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
	/* Should this return something else? */
	expect(mrpc_conn_close(conn), 0);
	mrpc_conn_free(conn);

	free_IntArray(NULL, 0);
	free_IntArray(NULL, 1);

	dispatcher_barrier();
	return 0;
}
