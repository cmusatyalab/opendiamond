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

static void *blast_main(void *arg)
{
	struct mrpc_connection *data_conn = arg;
	mrpc_dispatch_loop(data_conn);
	exit(0);

	return NULL;
}

void
connection_main(cstate_t *cstate)
{
	if (mrpc_conn_create(&cstate->mrpc_conn, rpc_client_content_server,
			     cstate->control_fd, cstate)) {
		printf("failed to create minirpc connection\n");
		return;
	}

	rpc_client_content_server_set_operations(cstate->mrpc_conn, sstub_ops);

	pthread_mutex_lock(&cstate->cmutex);
	cstate->flags &= ~CSTATE_CNTRL_FD;
	pthread_mutex_unlock(&cstate->cmutex);

	if (mrpc_conn_create(&cstate->blast_conn, blast_channel_server,
			     cstate->data_fd, cstate)) {
		printf("failed to create blast channel connection\n");
		return;
	}

	blast_channel_server_set_operations(cstate->blast_conn,sstub_blast_ops);

	pthread_mutex_lock(&cstate->cmutex);
	cstate->flags &= ~CSTATE_DATA_FD;
	pthread_mutex_unlock(&cstate->cmutex);

	pthread_t blast_thread;
	pthread_create(&blast_thread, NULL, blast_main, cstate->blast_conn);

	mrpc_dispatch_loop(cstate->mrpc_conn);
	exit(0);
}
