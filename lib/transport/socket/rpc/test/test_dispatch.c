/*
 * miniRPC - TCP RPC library with asynchronous operations
 *
 * Copyright (C) 2007-2008 Carnegie Mellon University
 *
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU Lesser General Public License version 2.1, as shown in
 * the file COPYING.
 */

#include <poll.h>
#include <pthread.h>
#include <semaphore.h>
#include "common.h"

sem_t done;

void *dispatcher_loop(void *setp)
{
	struct mrpc_conn_set *set=setp;

	mrpc_dispatcher_add(set);
	expect(mrpc_dispatch_loop(set), ENXIO);
	mrpc_dispatcher_remove(set);
	sem_post(&done);
	return NULL;
}

void *dispatcher_one(void *setp)
{
	struct mrpc_conn_set *set=setp;
	struct pollfd pfd;
	int ret;

	pfd.fd=mrpc_get_event_fd(set);
	pfd.events=POLLIN;
	mrpc_dispatcher_add(set);
	while (1) {
		ret=mrpc_dispatch(set, 1);
		switch (ret) {
		case 0:
			break;
		case ENXIO:
			goto out;
		case EAGAIN:
			poll(&pfd, 1, -1);
			break;
		default:
			die("mrpc_dispatch() returned %d", ret);
		}
	}
out:
	mrpc_dispatcher_remove(set);
	sem_post(&done);
	return NULL;
}

void *dispatcher_all(void *setp)
{
	struct mrpc_conn_set *set=setp;
	struct pollfd pfd;
	int ret;

	pfd.fd=mrpc_get_event_fd(set);
	pfd.events=POLLIN;
	mrpc_dispatcher_add(set);
	while (1) {
		ret=mrpc_dispatch(set, 0);
		switch (ret) {
		case 0:
			die("mrpc_dispatch(set, 0) returned 0");
		case ENXIO:
			goto out;
		case EAGAIN:
			poll(&pfd, 1, -1);
			break;
		default:
			die("mrpc_dispatch() returned %d", ret);
		}
	}
out:
	mrpc_dispatcher_remove(set);
	sem_post(&done);
	return NULL;
}

int main(int argc, char **argv)
{
	struct mrpc_conn_set *sset;
	struct mrpc_conn_set *cset;
	struct mrpc_connection *conn;
	char *port;
	int ret;
	int i;
	void *(*dispatch_func)(void *)=NULL;  /* avoid compiler warning */
	pthread_t thr;

	sem_init(&done, 0, 0);
	if (mrpc_conn_set_create(&cset, proto_client, NULL))
		die("Couldn't allocate conn set");
	mrpc_set_disconnect_func(cset, disconnect_user);
	start_monitored_dispatcher(cset);

	for (i=0; i<3; i++) {
		if (mrpc_conn_set_create(&sset, proto_server, NULL))
			die("Couldn't create conn set");
		mrpc_set_accept_func(sset, sync_server_accept);
		mrpc_set_disconnect_func(sset, disconnect_normal);
		port=NULL;
		if (mrpc_listen(sset, AF_INET, NULL, &port))
			die("Couldn't listen on socket");

		switch (i) {
		case 0:
			dispatch_func=NULL;
			break;
		case 1:
			dispatch_func=dispatcher_loop;
			break;
		case 2:
			dispatch_func=dispatcher_one;
			break;
		case 3:
			dispatch_func=dispatcher_all;
			break;
		}
		if (dispatch_func) {
			if (pthread_create(&thr, NULL, dispatch_func, sset))
				die("Couldn't create thread, iteration %d", i);
			pthread_detach(thr);
		} else {
			expect(mrpc_start_dispatch_thread(sset), 0);
		}

		ret=mrpc_conn_create(&conn, cset, NULL);
		if (ret)
			die("%s", strerror(ret));
		ret=mrpc_connect(conn, AF_UNSPEC, "localhost", port);
		if (ret)
			die("%s", strerror(ret));

		sync_client_set_ops(conn);
		sync_client_run(conn);
		trigger_callback_sync(conn);
		invalidate_sync(conn);
		mrpc_conn_close(conn);
		mrpc_conn_unref(conn);
		mrpc_listen_close(sset);
		mrpc_conn_set_unref(sset);
		if (dispatch_func)
			sem_wait(&done);
		free(port);
	}
	mrpc_conn_set_unref(cset);
	expect_disconnects(i, i, 0);
	sem_destroy(&done);
	return 0;
}
