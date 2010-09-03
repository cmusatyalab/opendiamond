/*
 * miniRPC - TCP RPC library with asynchronous operations
 *
 * Copyright (C) 2007-2008 Carnegie Mellon University
 *
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU Lesser General Public License version 2.1, as shown in
 * the file COPYING.
 */

#define FDLIMIT 100
#define FDCOUNT (FDLIMIT - 25)  /* buffer for overhead FDs */
#define MULTIPLE 5
#define DELAY 2

#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>
#include <glib.h>
#include <pthread.h>
#include "common.h"

struct open_conn {
	struct mrpc_connection *conn;
	struct timeval expire;
} sentinel;

void set_max_files(void)
{
	struct rlimit rlim;

	if (getrlimit(RLIMIT_NOFILE, &rlim))
		die("Couldn't get system fd limit");
	rlim.rlim_cur=FDLIMIT;
	if (setrlimit(RLIMIT_NOFILE, &rlim))
		die("Couldn't set system fd limit");
}

void *closer(void *arg)
{
	GAsyncQueue *queue=arg;
	struct open_conn *oconn;
	struct timeval curtime;
	struct timeval wait;
	struct timespec wait_ts;

	g_async_queue_ref(queue);
	while ((oconn=g_async_queue_pop(queue)) != &sentinel) {
		gettimeofday(&curtime, NULL);
		if (timercmp(&curtime, &oconn->expire, <)) {
			timersub(&oconn->expire, &curtime, &wait);
			TIMEVAL_TO_TIMESPEC(&wait, &wait_ts);
			while (nanosleep(&wait_ts, &wait_ts) &&
						errno == EINTR);
		}
		mrpc_conn_close(oconn->conn);
		mrpc_conn_unref(oconn->conn);
		g_slice_free(struct open_conn, oconn);
	}
	g_async_queue_unref(queue);
	return NULL;
}

void client(int sock)
{
	struct mrpc_conn_set *cset;
	struct mrpc_connection *conn;
	GAsyncQueue *queue;
	struct open_conn *oconn;
	pthread_t thr;
	int i;
	int ret;
	uint32_t port_i;
	char *port;

	if (mrpc_conn_set_create(&cset, proto_client, NULL))
		die("Couldn't allocate conn set");
	mrpc_set_disconnect_func(cset, disconnect_user);
	start_monitored_dispatcher(cset);
	queue=g_async_queue_new();
	pthread_create(&thr, NULL, closer, queue);

	/* Indicate readiness */
	if (write(sock, "a", 1) != 1)
		die("Short write");
	/* Wait until we're told to start, and what port number to use */
	if (read(sock, &port_i, 4) != 4)
		die("Short read");
	close(sock);
	port=g_strdup_printf("%u", port_i);
	for (i=0; i < FDCOUNT; i++) {
		ret=mrpc_conn_create(&conn, cset, NULL);
		if (ret)
			die("%s", strerror(ret));
		ret=mrpc_connect(conn, AF_UNSPEC, NULL, port);
		if (ret)
			die("%s", strerror(ret));
		/* Make sure the server has accepted the connection before
		   we close it, so that expect_disconnects() on the server
		   turns out right */
		expect(proto_ping(conn), 0);
		oconn=g_slice_new0(struct open_conn);
		oconn->conn=conn;
		gettimeofday(&oconn->expire, NULL);
		oconn->expire.tv_sec += DELAY;
		g_async_queue_push(queue, oconn);
	}
	g_async_queue_push(queue, &sentinel);
	pthread_join(thr, NULL);
	g_async_queue_unref(queue);
	mrpc_conn_set_unref(cset);
	expect_disconnects(FDCOUNT, 0, 0);
	g_free(port);
	exit(0);
}

int main(int argc, char **argv)
{
	struct mrpc_conn_set *sset;
	char *port;
	uint32_t port_i;
	int stat;
	int ret=0;
	int clients[MULTIPLE];
	int sock[2];
	int i;
	int j;

	/* Valgrind keeps a reserved FD range at the upper end of the FD
	   space, but doesn't use all of the FDs in it.  If accept() returns
	   an fd inside this space, Valgrind converts the return value into
	   EMFILE and closes the fd (!!!).  This causes the client to receive
	   unexpected connection closures and makes the test fail.  So we
	   don't run this test under Valgrind. */
	exclude_valgrind();

	set_max_files();

	for (i=0; i<MULTIPLE; i++) {
		if (socketpair(AF_UNIX, SOCK_STREAM, 0, sock))
			die("Couldn't create socket pair: %s",
						strerror(errno));
		clients[i]=sock[0];
		if (!fork()) {
			for (j = i; j >= 0; j--)
				close(clients[j]);
			client(sock[1]);
		}
		close(sock[1]);
	}
	/* Wait for clients to become ready */
	for (i=0; i<MULTIPLE; i++)
		if (read(clients[i], &j, 1) != 1)
			die("Short read");
	sset=spawn_server(&port, proto_server, sync_server_accept, NULL, 1);
	mrpc_set_disconnect_func(sset, disconnect_normal);
	port_i=atoi(port);
	/* Start them running */
	for (i=0; i<MULTIPLE; i++) {
		if (write(clients[i], &port_i, 4) != 4)
			die("Short write");
		close(clients[i]);
	}
	while (1) {
		if (wait(&stat) == -1) {
			if (errno == ECHILD)
				break;
			else
				continue;
		}
		if (WIFSIGNALED(stat)) {
			message("Client died on signal %d", WTERMSIG(stat));
			ret=1;
		}
		if (WIFEXITED(stat) && WEXITSTATUS(stat)) {
			message("Client returned %d", WEXITSTATUS(stat));
			ret=1;
		}
	}
	mrpc_listen_close(sset);
	mrpc_conn_set_unref(sset);
	expect_disconnects(0, MULTIPLE * FDCOUNT, 0);
	free(port);
	return ret;
}
