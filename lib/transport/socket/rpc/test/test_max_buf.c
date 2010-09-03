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

struct {
	pthread_mutex_t lock;
	pthread_cond_t cond;
} ready;

void ping_cb(void *conn_private, void *msg_private, mrpc_status_t status)
{
	if (status != MINIRPC_OK)
		die("Ping reply was %d", status);
	pthread_mutex_lock(&ready.lock);
	pthread_cond_broadcast(&ready.cond);
	pthread_mutex_unlock(&ready.lock);
}

/* We use async ping because it synchronizes both the client and the server
   event queues.  Otherwise we might call mrpc_conn_close() before all
   client-side ioerr events have been processed. */
void do_ping(struct mrpc_connection *conn)
{
	struct timespec ts = {0};

	pthread_mutex_lock(&ready.lock);
	expect(proto_ping_async(conn, ping_cb, NULL), 0);
	ts.tv_sec = time(NULL) + FAILURE_TIMEOUT;
	expect(pthread_cond_timedwait(&ready.cond, &ready.lock, &ts), 0);
	pthread_mutex_unlock(&ready.lock);
}

int main(int argc, char **argv)
{
	struct mrpc_conn_set *sset;
	struct mrpc_conn_set *cset;
	struct mrpc_connection *conn;
	char *port;
	int ret;

	pthread_mutex_init(&ready.lock, NULL);
	pthread_cond_init(&ready.cond, NULL);
	sset=spawn_server(&port, proto_server, sync_server_accept, NULL, 1);
	mrpc_set_disconnect_func(sset, disconnect_normal);
	mrpc_set_ioerr_func(sset, handle_ioerr);
	mrpc_set_max_buf_len(sset, 128);
	if (mrpc_conn_set_create(&cset, proto_client, NULL))
		die("Couldn't allocate conn set");
	mrpc_set_disconnect_func(cset, disconnect_user);
	mrpc_set_ioerr_func(cset, handle_ioerr);
	mrpc_set_max_buf_len(cset, 128);

	ret=mrpc_conn_create(&conn, cset, NULL);
	if (ret)
		die("%s", strerror(ret));
	ret=mrpc_connect(conn, AF_UNSPEC, "localhost", port);
	if (ret)
		die("%s", strerror(ret));

	start_monitored_dispatcher(cset);
	expect(send_buffer_sync(conn), MINIRPC_ENCODING_ERR);
	do_ping(conn);
	expect(recv_buffer_sync(conn), MINIRPC_ENCODING_ERR);
	do_ping(conn);
	msg_buffer_sync(conn);
	do_ping(conn);
	mrpc_conn_close(conn);
	mrpc_conn_unref(conn);
	mrpc_conn_set_unref(cset);
	mrpc_listen_close(sset);
	mrpc_conn_set_unref(sset);
	expect_disconnects(1, 1, 0);
	expect_ioerrs(3);
	free(port);
	return 0;
}
