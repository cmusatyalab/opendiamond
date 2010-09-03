/*
 * miniRPC - TCP RPC library with asynchronous operations
 *
 * Copyright (C) 2007-2008 Carnegie Mellon University
 *
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU Lesser General Public License version 2.1, as shown in
 * the file COPYING.
 */

#define SECS 3

#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include "common.h"

static volatile int done;

void alarm_handler(int unused)
{
	done=1;
}

int main(int argc, char **argv)
{
	struct mrpc_conn_set *sset;
	struct mrpc_conn_set *cset;
	struct mrpc_connection *conn;
	char *port;
	int ret;
	int i;
	struct sigaction act;

	memset(&act, 0, sizeof(act));
	act.sa_handler=alarm_handler;
	act.sa_flags=SA_RESTART;
	expect(sigaction(SIGALRM, &act, NULL), 0);

	sset=spawn_server(&port, proto_server, sync_server_accept, NULL, 1);
	mrpc_set_disconnect_func(sset, disconnect_normal);

	if (mrpc_conn_set_create(&cset, proto_client, NULL))
		die("Couldn't allocate conn set");
	mrpc_set_disconnect_func(cset, disconnect_user);
	start_monitored_dispatcher(cset);

	ret=mrpc_conn_create(&conn, cset, NULL);
	if (ret)
		die("%s", strerror(ret));
	ret=mrpc_connect(conn, AF_UNSPEC, "localhost", port);
	if (ret)
		die("%s", strerror(ret));

	/* Make sure the connection has completed on the server side */
	proto_ping(conn);
	alarm(SECS);
	for (i=0; !done; i++)
		expect(proto_ping(conn), 0);

	fprintf(stderr, "Throughput: %d RPCs/sec\n", i/SECS);

	mrpc_conn_close(conn);
	mrpc_conn_unref(conn);
	mrpc_conn_set_unref(cset);
	mrpc_listen_close(sset);
	mrpc_conn_set_unref(sset);
	expect_disconnects(1, 1, 0);
	free(port);
	return 0;
}
