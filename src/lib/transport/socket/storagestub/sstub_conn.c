/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 3
 *
 *  Copyright (c) 2002-2007 Intel Corporation
 *  Copyright (c) 2007-2008 Carnegie Mellon University
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

/*
 * These file handles a lot of the device specific code.  For the current
 * version we have state for each of the devices.
 */
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/time.h>
#include <netdb.h>
#include <string.h>
#include <assert.h>

#include <netinet/in.h>

#include "diamond_consts.h"
#include "diamond_types.h"
#include "lib_tools.h"
#include "obj_attr.h"
#include "lib_odisk.h"
#include "socket_trans.h"
#include "lib_dctl.h"
#include "lib_sstub.h"
#include "sstub_impl.h"

#include <minirpc/minirpc.h>
#include "rpc_client_content_server.h"

static struct mrpc_conn_set *conn_set;

static void disconnect_cb(void *conn_data, enum mrpc_disc_reason reason)
{
	shutdown_connection((cstate_t *)conn_data);
}

static int minirpc_init(void)
{
	if (mrpc_conn_set_create(&conn_set, rpc_client_content_server, NULL)) {
		printf("failed to create minirpc connection set\n");
		return -1;
	}

	if (mrpc_start_dispatch_thread(conn_set)) {
		printf("failed to start minirpc dispatch thread\n");
		goto err_out;
	}

	if (mrpc_set_disconnect_func(conn_set, disconnect_cb)) {
		printf("failed to initialize minirpc disconnect function\n");
		goto err_out;
	}

	return 0;

err_out:
	mrpc_conn_set_unref(conn_set);
	conn_set = NULL;
	return -1;

}

static int sstub_bind_conn(cstate_t *cstate)
{
	if (!conn_set && minirpc_init())
		return -1;

	if (mrpc_conn_create(&cstate->mrpc_conn, conn_set, cstate)) {
		printf("failed to create minirpc connection\n");
		return -1;
	}

	rpc_client_content_server_set_operations(cstate->mrpc_conn, sstub_ops);

	if (mrpc_bind_fd(cstate->mrpc_conn, cstate->control_fd)) {
		printf("failed to bind minirpc connection\n");
		return -1;
	}
	cstate->flags &= ~CSTATE_CNTRL_FD;
	return 0;
}

/*
 * This is the loop that handles the socket communications for each
 * of the different "connections" to the disk that exist.
 */
void
connection_main(cstate_t *cstate)
{
	struct timeval  to;
	int		max_fd;
	int		err;
	fd_set		read_fds;
	fd_set		write_fds;
	fd_set		except_fds;

	if (sstub_bind_conn(cstate))
		return;

	pthread_mutex_lock(&cstate->cmutex);
	cstate->flags |= CSTATE_ESTABLISHED;
	pthread_mutex_unlock(&cstate->cmutex);

	max_fd = cstate->data_fd + 1;

	while (1) {
		if (cstate->flags & CSTATE_SHUTTING_DOWN)
			break;

		FD_ZERO(&read_fds);
		FD_ZERO(&write_fds);
		FD_ZERO(&except_fds);

		FD_SET(cstate->data_fd, &read_fds);
		FD_SET(cstate->data_fd, &except_fds);

		pthread_mutex_lock(&cstate->cmutex);

		if ((cstate->flags & CSTATE_OBJ_DATA) &&
		    (cstate->cc_credits > 0)) {
			FD_SET(cstate->data_fd, &write_fds);
		}

		pthread_mutex_unlock(&cstate->cmutex);

		to.tv_sec = 1;
		to.tv_usec = 0;

		/*
		 * Sleep on the set of sockets to see if anything
		 * interesting has happened.
		 */
		err = select(max_fd, &read_fds, &write_fds, &except_fds, &to);

		if (err == -1) {
			/* XXX log */
			perror("XXX select failed ");
			break;
		}

		/*
		 * If err > 0 then there are some objects
		 * that have data.
		 */
		if (err > 0) {
			/* handle outgoing data on the data connection */
			if (FD_ISSET(cstate->data_fd, &read_fds))
				sstub_read_data(cstate);

			if (FD_ISSET(cstate->data_fd, &write_fds))
				sstub_write_data(cstate);

			if (FD_ISSET(cstate->data_fd, &except_fds))
				sstub_except_data(cstate);
		}
	}

	mrpc_conn_close(cstate->mrpc_conn);

	close(cstate->data_fd);

	pthread_mutex_lock(&cstate->cmutex);
	cstate->flags &= ~CSTATE_ESTABLISHED;
	cstate->flags &= ~CSTATE_DATA_FD;
	cstate->flags &= ~CSTATE_SHUTTING_DOWN;
	cstate->flags &= ~CSTATE_ALLOCATED;
	pthread_mutex_unlock(&cstate->cmutex);
	printf("exiting thread \n");
	pthread_exit((void *)0);
}
