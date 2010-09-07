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

static void disconnect_cb(void *conn_data, enum mrpc_disc_reason reason)
{
	exit(0);
}

static void *blast_main(void *arg)
{
	struct mrpc_conn_set *data_set = arg;
	mrpc_dispatcher_add(data_set);
	mrpc_dispatch_loop(data_set);
	exit(0);

	return NULL;
}

void
connection_main(cstate_t *cstate)
{
	struct mrpc_conn_set *conn_set;
	struct mrpc_conn_set *data_set;

	if (mrpc_conn_set_create(&conn_set, rpc_client_content_server, NULL) ||
	    mrpc_conn_set_create(&data_set, blast_channel_server, NULL))
	{
		printf("failed to create minirpc connection sets\n");
		return;
	}

	mrpc_dispatcher_add(conn_set);

	if (mrpc_set_disconnect_func(conn_set, disconnect_cb) ||
	    mrpc_set_disconnect_func(data_set, disconnect_cb))
	{
		printf("failed to initialize minirpc disconnect functions\n");
		return;
	}

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

	pthread_t blast_thread;
	pthread_create(&blast_thread, NULL, blast_main, data_set);

	mrpc_dispatch_loop(conn_set);
	exit(0);
}
