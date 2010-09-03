/*
 * miniRPC - TCP RPC library with asynchronous operations
 *
 * Copyright (C) 2007-2008 Carnegie Mellon University
 *
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU Lesser General Public License version 2.1, as shown in
 * the file COPYING.
 */

#include "common.h"

int main(int argc, char **argv)
{
	struct mrpc_conn_set *sset;
	struct mrpc_conn_set *cset;
	struct mrpc_connection *sconn;
	struct mrpc_connection *cconn;
	unsigned port;
	int listener;
	int sfd;
	int cfd;
	int ret;
	struct sockaddr_in addr;
	socklen_t addrlen=sizeof(addr);

	if (mrpc_conn_set_create(&sset, proto_server, NULL))
		die("Couldn't allocate conn set");
	if (mrpc_conn_set_create(&cset, proto_client, NULL))
		die("Couldn't allocate conn set");
	mrpc_set_accept_func(sset, sync_server_accept);
	mrpc_set_disconnect_func(sset, disconnect_user);
	mrpc_set_disconnect_func(cset, disconnect_normal);
	start_monitored_dispatcher(sset);
	start_monitored_dispatcher(cset);

	listener=socket(PF_INET, SOCK_STREAM, 0);
	if (listener == -1)
		die("Couldn't create socket");
	addr.sin_family=AF_INET;
	addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
	addr.sin_port=0;
	if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)))
		die("Couldn't bind socket: %s", strerror(errno));
	if (listen(listener, 16))
		die("Couldn't listen on socket");
	if (getsockname(listener, (struct sockaddr *)&addr, &addrlen))
		die("Couldn't get socket name");
	port=addr.sin_port;

	cfd=socket(PF_INET, SOCK_STREAM, 0);
	if (cfd == -1)
		die("Couldn't create socket");
	addr.sin_family=AF_INET;
	addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
	addr.sin_port=port;
	if (connect(cfd, (struct sockaddr *)&addr, sizeof(addr)))
		die("Couldn't open connection");

	sfd=accept(listener, NULL, NULL);
	if (sfd == -1)
		die("Couldn't accept connection");

	/* Deliberately reverse client and server */
	ret=mrpc_conn_create(&sconn, sset, NULL);
	if (ret)
		die("Couldn't create server conn: %d", ret);
	sync_server_set_ops(sconn);
	ret=mrpc_bind_fd(sconn, cfd);
	if (ret)
		die("Couldn't bind server fd: %d", ret);
	ret=mrpc_conn_create(&cconn, cset, NULL);
	if (ret)
		die("Couldn't create client conn: %d", ret);
	ret=mrpc_bind_fd(cconn, sfd);
	if (ret)
		die("Couldn't bind client fd: %d", ret);
	sync_client_run(cconn);
	mrpc_conn_close(sconn);
	mrpc_conn_unref(sconn);
	mrpc_conn_set_unref(sset);
	mrpc_conn_set_unref(cset);
	expect_disconnects(1, 1, 0);
	return 0;
}
