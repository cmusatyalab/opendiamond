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

#ifndef	_SSTUB_IMPL_H_
#define	_SSTUB_IMPL_H_

#include <minirpc/minirpc.h>

/* the max concurrent connections that we currently support */
#define	MAX_CONNS		64

/* These are the flags for each connection state defined below */
#define	CSTATE_ALLOCATED	0x0001
#define	CSTATE_CNTRL_FD		0x0002
#define	CSTATE_DATA_FD		0x0004
#define	CSTATE_ALL_FD		(CSTATE_CNTRL_FD|CSTATE_DATA_FD)
#define	CSTATE_ESTABLISHED	0x0010
#define	CSTATE_SHUTTING_DOWN	0x0020

#define	CSTATE_CONTROL_DATA	0x0100	/* control messages pending */
#define	CSTATE_OBJ_DATA		0x0200	/* data objects pending */


/*
 * This is the structure that holds the state for each of the conneciton
 * the storage device.  This should roughly correspond to a search
 * context (I.e each search will have a connection to each device 
 * that is involved in the search).
 */
typedef enum {
    DATA_TX_NO_PENDING,
    DATA_TX_HEADER,
    DATA_TX_ATTR,
    DATA_TX_DATA,
} data_tx_state_t;

typedef enum {
    NW_ATTR_POLICY_FIXED = 0,
    NW_ATTR_POLICY_PROPORTIONAL,
    NW_ATTR_POLICY_QUEUE
} nw_attr_policy;


#define DESIRED_MAX_TX_QUEUE    20
#define DESIRED_MAX_TX_THRESH   10
#define DESIRED_MAX_CREDITS    	8
#define DESIRED_CREDIT_THRESH  	6

#define DEFAULT_NW_ATTR_POLICY  (NW_ATTR_POLICY_FIXED)
#define DEFAULT_NW_ATTR_RATIO   (100)


/* XXX forward ref */
struct listener_state;

typedef struct cstate {
	sig_val_t		nonce;
	unsigned int		flags;
	pthread_t		thread_id;
	pthread_mutex_t		cmutex;
	struct listener_state	*lstate;
	session_info_t		cinfo;
	struct mrpc_connection	*mrpc_conn;
	int			control_fd;
	int			data_fd;
	int			pend_obj;
	int			have_start;
	void *			app_cookie;
	ring_data_t *		complete_obj_ring;
	ring_data_t *		partial_obj_ring;
	obj_data_t *		data_tx_obj;
	data_tx_state_t		data_tx_state;
	int		        data_tx_offset;
	obj_header_t		data_tx_oheader;
	int			attr_policy;
	unsigned int		attr_threshold;
	int			attr_ratio;
	int			drop_attrs;
	unsigned char *		attr_buf;
	void *			attr_cookie;
	size_t			attr_remain;
	/* store incoming credit message */
	credit_count_msg_t	cc_msg;
	/* number of remaining credits */
	uint32_t		cc_credits;
	uint32_t            	stats_objs_tx;
	uint64_t            	stats_objs_attr_bytes_tx;
	uint64_t            	stats_objs_data_bytes_tx;
	uint64_t            	stats_objs_hdr_bytes_tx;
	uint64_t            	stats_objs_total_bytes_tx;
	uint32_t            	stats_control_tx;
	uint64_t            	stats_control_bytes_tx;
	uint32_t            	stats_control_rx;
	uint64_t            	stats_control_bytes_rx;
	uint32_t            	stats_log_tx;
	uint64_t            	stats_log_bytes_tx;
}
cstate_t;

/*
 * This is the main state for the library.  It includes the socket
 * state for each of the "listners" as well as all the callback
 * functions that are invoked messages of a specified type arrive.
 */

typedef struct listener_state {
	pthread_t		thread_id;
	int			listen_fd;
	sstub_cb_args_t		cb;
	cstate_t		conns[MAX_CONNS];
	struct mrpc_conn_set	*set;
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
void shutdown_connection(listener_state_t *lstate, cstate_t *cstate);
int sstub_new_sock(int *fd, const char *port, int bind_only_locally);

/*
 * Functions exported by sstub_cntrl.c
 */
int sstub_bind_conn(cstate_t *new_cstate);

/*
 * Functions exported by sstub_data.c
 */
void sstub_write_data(cstate_t *cstate);
void sstub_read_data(cstate_t *cstate);
void sstub_except_data(cstate_t *cstate);

/*
 * Functions exported by sstub_conn.c
 */
void connection_main(listener_state_t *lstate, int conn);

#endif /* !_SSTUB_IMPL_H_ */
