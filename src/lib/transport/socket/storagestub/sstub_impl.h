/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 3
 *
 *  Copyright (c) 2002-2007 Intel Corporation
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
 *  Copyright (c) 2006-2007 Carnegie Mellon University
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#ifndef	_SSTUB_IMPL_H_
#define	_SSTUB_IMPL_H_


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
    CONTROL_RX_NO_PENDING,
    CONTROL_RX_HEADER,
    CONTROL_RX_DATA
} control_rx_state_t;


typedef enum {
    CONTROL_TX_NO_PENDING,
    CONTROL_TX_HEADER,
    CONTROL_TX_DATA
} control_tx_state_t;


typedef enum {
    DATA_TX_NO_PENDING,
    DATA_TX_HEADER,
    DATA_TX_ATTR,
    DATA_TX_DATA,
} data_tx_state_t;

typedef enum {
    LOG_TX_NO_PENDING,
    LOG_TX_HEADER,
    LOG_TX_DATA,
} log_tx_state_t;

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
	unsigned int		flags;
	pthread_t		    thread_id;
	pthread_mutex_t		cmutex;
	struct listener_state *lstate;
	session_info_t		cinfo;
	int			        control_fd;
	int			        data_fd;
        int                             tirpc_fd;
	int			        pend_obj;
	int			        have_start;
	uint32_t		    start_gen;
	void *			app_cookie;
	fd_set			read_fds;
	fd_set			write_fds;
	fd_set			except_fds;
	ring_data_t *		complete_obj_ring;
	ring_data_t *		partial_obj_ring;
	control_rx_state_t	control_rx_state;
	char *			control_rx_data;
	int			control_rx_offset;
	ring_data_t * 		control_tx_ring;
	control_tx_state_t	control_tx_state;
	int			control_tx_offset;
	char *			log_tx_buf;
	int		        log_tx_len;
	int		        log_tx_offset;
	log_header_t		log_tx_header;
	log_tx_state_t		log_tx_state;
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

/* These are the flags for each listener state defined below */
#define	LSTATE_AUTH_REQUIRED	0x0001


/*
 * This is the main state for the library.  It includes the socket
 * state for each of the "listners" as well as all the callback
 * functions that are invoked messages of a specified type arrive.
 */

typedef struct listener_state {
	pthread_t		thread_id;
	int			control_fd;
	int			data_fd;
	unsigned int		flags;
	fd_set			read_fds;
	fd_set			write_fds;
	fd_set			except_fds;
	auth_handle_t		ca_handle;
	auth_handle_t		da_handle;
	auth_handle_t		la_handle;
	sstub_new_conn_fn 	new_conn_cb;
	sstub_close_conn_fn 	close_conn_cb;
	sstub_start_fn 		start_cb;
	sstub_stop_fn 		stop_cb;
	sstub_set_filter_spec_fn set_fspec_cb;
	sstub_set_filter_obj_fn set_fobj_cb;
	sstub_terminate_fn	terminate_cb;
	sstub_getstats_fn	get_stats_cb;
	sstub_release_obj_fn	release_obj_cb;
	sstub_get_devchar_fn	get_char_cb;
	sstub_log_done_fn	log_done_cb;
	sstub_set_log_fn	setlog_cb;
	sstub_rleaf_fn	        rleaf_cb;
	sstub_wleaf_fn	        wleaf_cb;
	sstub_lleaf_fn	        lleaf_cb;
	sstub_lnode_fn	        lnode_cb;
	sstub_sgid_fn	        sgid_cb;
	sstub_clear_gids_fn	clear_gids_cb;
	sstub_set_blob_fn	set_blob_cb;
	sstub_set_exec_mode_fn set_exec_mode_cb;
	sstub_set_user_state_fn set_user_state_cb;
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
void shutdown_connection(listener_state_t *lstate, cstate_t *cstate);
int sstub_new_sock(int *fd, int port, int bind_only_locally);

/*
 * Functions exported by sstub_cntrl.c
 */
void sstub_read_control(listener_state_t *lstate, cstate_t *cstate);
void sstub_except_control(listener_state_t *lstate, cstate_t *cstate);

/*
 * Functions exported by sstub_log.c
 */
void sstub_write_log(listener_state_t *lstate, cstate_t *cstate);
void sstub_read_log(listener_state_t *lstate, cstate_t *cstate);
void sstub_except_log(listener_state_t *lstate, cstate_t *cstate);

/*
 * Functions exported by sstub_data.c
 */
void sstub_write_data(listener_state_t *lstate, cstate_t *cstate);
void sstub_read_data(listener_state_t *lstate, cstate_t *cstate);
void sstub_except_data(listener_state_t *lstate, cstate_t *cstate);

/*
 * Functions exported by sstub_tirpc.c
 */
void sstub_read_tirpc(listener_state_t *lstate, cstate_t *cstate);
void sstub_except_tirpc(listener_state_t *lstate, cstate_t *cstate);

/*
 * Functions exported by sstub_conn.c
 */
void * connection_main(listener_state_t *lstate, int conn);

extern cstate_t *tirpc_cstate;
extern listener_state_t *tirpc_lstate;

#endif /* !_SSTUB_IMPL_H_ */
