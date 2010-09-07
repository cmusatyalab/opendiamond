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
#include <unistd.h>
#include "common.h"

int main(int argc, char **argv)
{
	struct mrpc_connection *sconn;
	struct mrpc_connection *conn;
	int sock[2];

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sock))
		die("%s", strerror(errno));

	if (mrpc_conn_create(&sconn, proto_server, sock[0], NULL))
		die("Couldn't allocate conn");
	sync_server_set_ops(sconn);
	start_monitored_dispatcher(sconn);
	if (mrpc_conn_create(&conn, proto_client, sock[1], NULL))
		die("Couldn't allocate conn");

	sync_client_set_ops(conn);
	sync_client_run(conn);
	trigger_callback_sync(conn);
	send_buffer_sync(conn);
	recv_buffer_sync(conn);
	mrpc_conn_close(conn);
	mrpc_conn_free(conn);
	dispatcher_barrier();
	return 0;
}
