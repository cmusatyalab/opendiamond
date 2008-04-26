/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 3
 *
 *  Copyright (c) 2002-2007 Intel Corporation
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
 *  Copyright (c) 2007-2008 Carnegie Mellon University
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#ifndef	_LIB_HSTUB_IMPL_H_
#define	_LIB_HSTUB_IMPL_H_

#include <rpc/rpc.h>
#include "lib_auth.h"

/*
 * This structure keeps track of the state associated with each
 * of the storage devices.
 */

typedef enum {
    DATA_RX_NO_PENDING,
    DATA_RX_HEADER,
    DATA_RX_ATTR,
    DATA_RX_DATA,
} data_rx_state_t;


/* flag definitons */
#define	CINFO_PENDING_CONTROL	0x01
#define	CINFO_BLOCK_OBJ		0x02
#define	CINFO_PENDING_CREDIT	0x04
#define	CINFO_DOWN		0x08
#define CINFO_AUTHENTICATED	0x10

typedef struct conn_info {
	int	 		flags;
	uint32_t		ipv4addr;
	pthread_mutex_t		mutex;
        uint32_t		session_nonce; /* for pairing control and data conns */
	int			control_fd;
	int			data_fd;
	data_rx_state_t		data_rx_state;
	obj_header_t		data_rx_header;
	int			data_rx_offset;
	obj_data_t *		data_rx_obj;
	CLIENT *                rpc_client;
	pthread_mutex_t         rpc_mutex;
	credit_count_msg_t 	cc_msg;
	int			cc_counter;
	int			obj_limit;
	fd_set			read_fds;
	fd_set			write_fds;
	fd_set			except_fds;
	auth_handle_t			ca_handle;
	auth_handle_t			da_handle;
	auth_handle_t			la_handle;
	uint32_t            	stat_log_rx;
	uint64_t            	stat_log_byte_rx;
	uint32_t            	stat_control_rx;
	uint64_t            	stat_control_byte_rx;
	uint32_t            	stat_control_tx;
	uint64_t            	stat_control_byte_tx;
	uint32_t            	stat_obj_rx;
	uint64_t            	stat_obj_total_byte_rx;
	uint64_t            	stat_obj_hdr_byte_rx;
	uint64_t            	stat_obj_attr_byte_rx;
	uint64_t            	stat_obj_data_byte_rx;
} conn_info_t;


typedef struct sdevice_state {
	struct sdevice_state * 	next;
	pthread_t	        thread_id;
	ring_data_t *	    	obj_ring;
	conn_info_t 	    	con_data;
	int		      	ver_no;
	void *		    	hcookie;
	device_char_t		dev_char;	/* cached device chars */
	int		       	stat_size;	/* size of caches stats */
	dev_stats_t *		dstats;		/* caches stats */
	void *			dctl_cookie;
	void *			log_cookie;
	hstub_cb_args_t		cb;
} sdevice_state_t;

/*
 * Functions availabe in hstub_api.c
 */
int rpc_preproc(const char *func, struct conn_info *con);
int rpc_postproc(const char *func, struct conn_info *con,
		 enum clnt_stat rpc_rc, diamond_rc_t *rc);

/*
 * Functions availabe in hstub_main.c
 */
void * hstub_main(void *arg);
void hstub_conn_down(sdevice_state_t *dev);

/*
 * Functions available in hstub_cntrl.c
 */
void hstub_read_cntrl(sdevice_state_t *dev);
void hstub_except_cntrl(sdevice_state_t *dev);
void hstub_write_cntrl(sdevice_state_t *dev);

/*
 * Functions available in hstub_data.c
 */
void hstub_read_data(sdevice_state_t *dev);
void hstub_except_data(sdevice_state_t *dev);
void hstub_write_data(sdevice_state_t *dev);

/*
 * Functions available in hstub_socket.h.
 */
int hstub_establish_connection(conn_info_t *cinfo, const char *host);

#endif	/* !_LIB_HSTUB_IMPL_H_ */



