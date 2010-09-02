/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 4
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

#ifndef	_SSTUB_IMPL_H_
#define	_SSTUB_IMPL_H_

#include <semaphore.h>
#include <glib.h>
#include "ring.h"

#include <minirpc/minirpc.h>
#include "rpc_client_content_server.h"

/* the max concurrent connections that we currently support */
#define	MAX_CONNS		64

/* These are the flags for each connection state defined below */
#define	CSTATE_ALLOCATED	0x0001
#define	CSTATE_CNTRL_FD		0x0002
#define	CSTATE_DATA_FD		0x0004
#define	CSTATE_ALL_FD		(CSTATE_CNTRL_FD|CSTATE_DATA_FD)

/*
 * This is the structure that holds the state for each of the connections
 * to the storage device.  This should roughly correspond to a search
 * context (I.e each search will have a connection to each device 
 * that is involved in the search).
 */

/* XXX forward ref */
struct listener_state;

typedef struct cstate {
	sig_val_t		nonce;
	unsigned int		flags;
	pthread_mutex_t		cmutex;
	sem_t			shutdown;
	struct listener_state	*lstate;
	session_info_t		cinfo;
	struct mrpc_connection	*mrpc_conn;
	int			control_fd;
	struct mrpc_connection	*blast_conn;
	int			data_fd;
	unsigned int		search_id;
	int			pend_obj;
	int			have_start;
	void *			app_cookie;
	ring_data_t *		complete_obj_ring;
	GArray *		thumbnail_set;
	/* number of remaining credits */
	uint32_t		cc_credits;
}
cstate_t;

/*
 * This is the main state for the library.  It includes the socket
 * state for each of the "listners" as well as all the callback
 * functions that are invoked messages of a specified type arrive.
 */

typedef struct listener_state {
	int			listen_fd;
	sstub_cb_args_t		cb;
	cstate_t		conns[MAX_CONNS];
} listener_state_t;


/*
 * These are some constants we use when creating temporary files
 * to hold the searchlet file as well as the filter spec.
 */

#define	MAX_TEMP_NAME	64

#define	TEMP_DIR_NAME	"/tmp/"
#define	TEMP_OBJ_NAME	"objfileXXXXXX"
#define	TEMP_SPEC_NAME	"fspecXXXXXX"


/*
 * Functions exported by sstub_listen.c
 */
void shutdown_connection(cstate_t *cstate);
int sstub_new_sock(int *fd, const char *port, int bind_only_locally);

/*
 * Functions exported by sstub_cntrl.c
 */
/* minirpc server operations */
const struct rpc_client_content_server_operations *sstub_ops;

/*
 * Functions exported by sstub_data.c
 */
const struct blast_channel_server_operations *sstub_blast_ops;
void sstub_send_objects(cstate_t *cstate);

/*
 * Functions exported by sstub_conn.c
 */
void connection_main(cstate_t *cstate);


/*
 * Other private functions
 */
int sstub_get_attributes(obj_data_t *obj, GArray *result_set,
			 attribute_x **result_val, unsigned int *result_len);

#endif /* !_SSTUB_IMPL_H_ */
