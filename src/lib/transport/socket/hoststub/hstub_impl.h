/*
 *
 *
 *                          Diamond 1.0
 * 
 *            Copyright (c) 2002-2004, Intel Corporation
 *                         All Rights Reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 *    * Neither the name of Intel nor the names of its contributors may
 *      be used to endorse or promote products derived from this software 
 *      without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
/* flag definitons */
#define	CINFO_PENDING_CONTROL	0x01
#define	CINFO_BLOCK_OBJ			0x02
#define	CINFO_PENDING_CREDIT	0x04

typedef struct conn_info {
	int			        flags;
	uint32_t		    dev_id;
	pthread_mutex_t		mutex;
	uint32_t		    con_cookie;
	int			        control_fd;
	control_tx_state_t	control_state;
	control_header_t *	control_header;		
	int			        control_offset;
	control_header_t	control_rx_header;	/* hdr being recieved */
	control_rx_state_t	control_rx_state;	/* recieve state */
	int			        control_rx_offset;	/* current rx offset  */
	char *			    control_rx_data;	/* rx data buffer */ 
	int			        data_fd;
	data_rx_state_t		data_rx_state;
	obj_header_t		data_rx_header;
	int			        data_rx_offset;
	obj_data_t *		data_rx_obj;
	log_rx_state_t		log_rx_state;
	int			        log_rx_offset;
	char *			    log_rx_data;
	log_header_t		log_rx_header;
	credit_count_msg_t 	cc_msg;
	int			        obj_limit;
	int			        log_fd;				/* file descriptor for the logging connection */
	fd_set			    read_fds;
	fd_set			    write_fds;
	fd_set			    except_fds;
    uint32_t            stat_log_rx;
    uint64_t            stat_log_byte_rx;
    uint32_t            stat_control_rx;
    uint64_t            stat_control_byte_rx;
    uint32_t            stat_control_tx;
    uint64_t            stat_control_byte_tx;
    uint32_t            stat_obj_rx;
    uint64_t            stat_obj_total_byte_rx;
    uint64_t            stat_obj_hdr_byte_rx;
    uint64_t            stat_obj_attr_byte_rx;
    uint64_t            stat_obj_data_byte_rx;
} conn_info_t;



#define	CI_NO_PENDING	0
#define	CI_GET_ATTR	1
#define	CI_GET_DATA	2


typedef struct sdevice_state {
	struct sdevice_state * 	next;
	pthread_t		        thread_id;
	ring_data_t *		    device_ops;	
	ring_data_t *		    obj_ring;	
	conn_info_t 		    con_data;
	unsigned int		    flags;
	int			      ver_no;
	void *		       	     hcookie;
	device_char_t		    dev_char;	/* cached device chars */
	int		            stat_size;	/* size of caches stats */
	dev_stats_t *		    dstats;		/* caches stats */
	void *				dctl_cookie;
	void *				log_cookie;
	hstub_log_data_fn	    hstub_log_data_cb;
	hstub_search_done_fn	hstub_search_done_cb;
	hstub_wleaf_done_fn	    hstub_wleaf_done_cb;
	hstub_rleaf_done_fn	    hstub_rleaf_done_cb;
	hstub_lnodes_done_fn    hstub_lnode_done_cb;
	hstub_lleafs_done_fn    hstub_lleaf_done_cb;
} sdevice_state_t;


/*
 * Functions availabe in hstub_main.c
 */
void * hstub_main(void *arg);

/*
 * Functions availabel in hstub_log.c
 */
void hstub_read_log(sdevice_state_t *dev);
void hstub_except_log(sdevice_state_t *dev);
void hstub_write_log(sdevice_state_t *dev);

/*
 * Functions availabel in hstub_cntrl.c
 */
void hstub_read_cntrl(sdevice_state_t *dev);
void hstub_except_cntrl(sdevice_state_t *dev);
void hstub_write_cntrl(sdevice_state_t *dev);

/*
 * Functions availabel in hstub_data.c
 */
void hstub_read_data(sdevice_state_t *dev);
void hstub_except_data(sdevice_state_t *dev);
void hstub_write_data(sdevice_state_t *dev);

/*
 * Functions available in hstub_socket.h.
 */
int hstub_establish_connection(conn_info_t *cinfo, uint32_t devid);





#endif	/* _LIB_HSTUB_IMPL_H_ */



