/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 3
 *
 *  Copyright (c) 2002-2007 Intel Corporation
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
 *  Copyright (c) 2006-2008 Carnegie Mellon University
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
#include "diamond_consts.h"
#include "diamond_types.h"
#include "lib_tools.h"
#include "obj_attr.h"
#include "lib_odisk.h"
#include "socket_trans.h"
#include "dctl_impl.h"
#include "lib_sstub.h"
#include "sstub_impl.h"
#include "ports.h"
#include "tools_priv.h"

/*
 * XXX do we manage the complete ring also?? 
 */
float
sstub_get_drate(void *cookie)
{

	cstate_t       *cstate;
	cstate = (cstate_t *) cookie;

	return (ring_drate(cstate->partial_obj_ring));
}


void sstub_get_conn_info(void *cookie, session_info_t *cinfo) {

	cstate_t       *cstate;

	cstate = (cstate_t *) cookie;
	memcpy((void *)cinfo, (void *)&cstate->cinfo, sizeof(session_info_t));
	
	return;
}


/*
 * Send an object.
 *
 * return current queue depth??
 */
int
sstub_send_obj(void *cookie, obj_data_t * obj, int complete)
{
	cstate_t	*cstate;
	int		err;

	cstate = (cstate_t *) cookie;

	/*
	 * Set a flag to indicate there is object
	 * data associated with our connection.
	 */
	/*
	 * XXX log
	 */
	pthread_mutex_lock(&cstate->cmutex);
	if (complete) {
		err = ring_enq(cstate->complete_obj_ring, obj);
	} else {
		err = ring_enq(cstate->partial_obj_ring, obj);
	}
	pthread_mutex_unlock(&cstate->cmutex);

	if (!err)
	    sstub_send_objects(cstate);

	return err;
}

int
sstub_get_partial(void *cookie, obj_data_t **obj)
{
	cstate_t	*cstate;

	cstate = (cstate_t *) cookie;

	/*
	 * Set a flag to indicate there is object
	 * data associated with our connection.
	 */
	/*
	 * XXX log
	 */
	pthread_mutex_lock(&cstate->cmutex);
	*obj = ring_deq(cstate->partial_obj_ring);
	pthread_mutex_unlock(&cstate->cmutex);

	return (*obj == NULL);
}

int
sstub_flush_objs(void *cookie)
{
	cstate_t	*cstate;
	obj_data_t	*obj;
	listener_state_t *lstate;

	cstate = (cstate_t *) cookie;
	lstate = cstate->lstate;

	/*
	 * Set a flag to indicate there is object
	 * data associated with our connection.
	 */
	/*
	 * XXX log
	 */
	while (1) {
		pthread_mutex_lock(&cstate->cmutex);
		obj = ring_deq(cstate->complete_obj_ring);
		pthread_mutex_unlock(&cstate->cmutex);

		/* we got through them all */
		if (!obj) break;

		(*lstate->cb.release_obj_cb) (cstate->app_cookie, obj);
	}

	while (1) {
		pthread_mutex_lock(&cstate->cmutex);
		obj = ring_deq(cstate->partial_obj_ring);
		pthread_mutex_unlock(&cstate->cmutex);

		/* we got through them all */
		if (!obj) return 0;

		(*lstate->cb.release_obj_cb) (cstate->app_cookie, obj);
	}
	return (0);
}


/*
 * This is the initialization function that is called by adiskd when we
 * start up. The second argument can be used to bind only to localhost.
 */
void *
sstub_init(sstub_cb_args_t *cb_args, int bind_only_locally)
{
	listener_state_t *lstate;
	int err;

	lstate = (listener_state_t *) calloc(1, sizeof(*lstate));
	if (lstate == NULL) {
		return (NULL);
	}

	/* Save all the callback functions. */
	lstate->cb = *cb_args;

	/* Open the listener sockets for the different types of connections. */
	err = sstub_new_sock(&lstate->listen_fd, diamond_get_control_port(),
			     bind_only_locally);
	if (err) {
		/* XXX log */
		printf("failed to create listener\n");
		free(lstate);
		return NULL;
	}
	return lstate;
}
