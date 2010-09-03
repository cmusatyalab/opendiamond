/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 4
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
#include "lib_sstub.h"
#include "sstub_impl.h"

#include <minirpc/minirpc.h>
#include "rpc_client_content_server.h"
#include "blast_channel_server.h"

static struct mrpc_conn_set *conn_set;
static struct mrpc_conn_set *data_set;

static void disconnect_cb(void *conn_data, enum mrpc_disc_reason reason)
{
	cstate_t *cstate = (cstate_t *)conn_data;
	sem_post(&cstate->shutdown);
}

static int minirpc_init(void)
{
	if (mrpc_conn_set_create(&conn_set, rpc_client_content_server, NULL) ||
	    mrpc_conn_set_create(&data_set, blast_channel_server, NULL))
	{
		printf("failed to create minirpc connection sets\n");
		goto err_out;
	}

	if (mrpc_start_dispatch_thread(conn_set) ||
	    mrpc_start_dispatch_thread(data_set))
	{
		printf("failed to start minirpc dispatch threads\n");
		goto err_out;
	}

	if (mrpc_set_disconnect_func(conn_set, disconnect_cb) ||
	    mrpc_set_disconnect_func(data_set, disconnect_cb))
	{
		printf("failed to initialize minirpc disconnect functions\n");
		goto err_out;
	}
	return 0;

err_out:
	mrpc_conn_set_unref(conn_set);
	mrpc_conn_set_unref(data_set);
	conn_set = data_set = NULL;
	return -1;
}

void
connection_main(cstate_t *cstate)
{
	if (!conn_set && minirpc_init())
		return;

	if (mrpc_conn_create(&cstate->mrpc_conn, conn_set, cstate)) {
		printf("failed to create minirpc connection\n");
		return;
	}

	rpc_client_content_server_set_operations(cstate->mrpc_conn, sstub_ops);

	if (mrpc_bind_fd(cstate->mrpc_conn, cstate->control_fd)) {
		printf("failed to bind minirpc connection\n");
		return;
	}

	pthread_mutex_lock(&cstate->cmutex);
	cstate->flags &= ~CSTATE_CNTRL_FD;
	pthread_mutex_unlock(&cstate->cmutex);

	if (mrpc_conn_create(&cstate->blast_conn, data_set, cstate)) {
		printf("failed to create blast channel connection\n");
		return;
	}

	blast_channel_server_set_operations(cstate->blast_conn,sstub_blast_ops);

	if (mrpc_bind_fd(cstate->blast_conn, cstate->data_fd)) {
		printf("failed to bind blast channel connection\n");
		return;
	}

	pthread_mutex_lock(&cstate->cmutex);
	cstate->flags &= ~CSTATE_DATA_FD;
	pthread_mutex_unlock(&cstate->cmutex);

	sem_wait(&cstate->shutdown);
}
