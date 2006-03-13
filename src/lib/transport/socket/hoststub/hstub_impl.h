/*
 * 	Diamond (Release 1.0)
 *      A system for interactive brute-force search
 *
 *      Copyright (c) 2002-2005, Intel Corporation
 *      All Rights Reserved
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

/*
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */


#ifndef	_LIB_HSTUB_IMPL_H_
#define	_LIB_HSTUB_IMPL_H_


/*
 * This structure keeps track of the state associated with each
 * of the storage devices.
 */

typedef enum {
    CONTROL_TX_NO_PENDING,
    CONTROL_TX_HEADER,
    CONTROL_TX_DATA
} control_tx_state_t;


typedef enum {
    CONTROL_RX_NO_PENDING,
    CONTROL_RX_HEADER,
    CONTROL_RX_DATA
} control_rx_state_t;


typedef enum {
    DATA_RX_NO_PENDING,
    DATA_RX_HEADER,
    DATA_RX_ATTR,
    DATA_RX_DATA,
} data_rx_state_t;

typedef enum {
    LOG_RX_NO_PENDING,
    LOG_RX_HEADER,
    LOG_RX_DATA
} log_rx_state_t;



/* flag definitons */
#define	CINFO_PENDING_CONTROL	0x01
#define	CINFO_BLOCK_OBJ		0x02
#define	CINFO_PENDING_CREDIT	0x04
#define	CINFO_DOWN		0x08

typedef struct conn_info {
	int	 		flags;
	uint32_t		dev_id;
	pthread_mutex_t		mutex;
	uint32_t		con_cookie;
	int			control_fd;
	control_tx_state_t	control_state;
	control_header_t *	control_header;
	int			control_offset;
	control_header_t	control_rx_header;	/* hdr being recieved */
	control_rx_state_t	control_rx_state;	/* recieve state */
	int			control_rx_offset;	/* current rx offset  */
	char *			control_rx_data;	/* rx data buffer */
	int			data_fd;
	data_rx_state_t		data_rx_state;
	obj_header_t		data_rx_header;
	int			data_rx_offset;
	obj_data_t *		data_rx_obj;
	log_rx_state_t		log_rx_state;
	int			log_rx_offset;
	char *			log_rx_data;
	log_header_t		log_rx_header;
	credit_count_msg_t 	cc_msg;
	int			obj_limit;
	int			log_fd;		/* fd for the log connection */
	fd_set			read_fds;
	fd_set			write_fds;
	fd_set			except_fds;
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



#define	CI_NO_PENDING	0
#define	CI_GET_ATTR	1
#define	CI_GET_DATA	2
#define	CI_GET_DATA	2


typedef struct sdevice_state {
	struct sdevice_state * 	next;
	pthread_t	        thread_id;
	ring_data_t *	    	device_ops;
	ring_data_t *	    	obj_ring;
	conn_info_t 	    	con_data;
	unsigned int	    	flags;
	int		      	ver_no;
	void *		    	hcookie;
	device_char_t		dev_char;	/* cached device chars */
	int		       	stat_size;	/* size of caches stats */
	dev_stats_t *		dstats;		/* caches stats */
	void *			dctl_cookie;
	void *			log_cookie;
	hstub_log_data_fn	hstub_log_data_cb;
	hstub_search_done_fn	hstub_search_done_cb;
	hstub_conn_down_fn	hstub_conn_down_cb;
	hstub_wleaf_done_fn	hstub_wleaf_done_cb;
	hstub_rleaf_done_fn	hstub_rleaf_done_cb;
	hstub_lnodes_done_fn    hstub_lnode_done_cb;
	hstub_lleafs_done_fn    hstub_lleaf_done_cb;
} sdevice_state_t;


/*
 * Functions availabe in hstub_main.c
 */
void * hstub_main(void *arg);
void hstub_conn_down(sdevice_state_t *dev);

/*
 * Functions available in hstub_log.c
 */
void hstub_read_log(sdevice_state_t *dev);
void hstub_except_log(sdevice_state_t *dev);
void hstub_write_log(sdevice_state_t *dev);

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
int hstub_establish_connection(conn_info_t *cinfo, uint32_t devid);


#endif	/* !_LIB_HSTUB_IMPL_H_ */



