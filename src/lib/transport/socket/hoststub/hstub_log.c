/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 2
 *
 *  Copyright (c) 2002-2007 Intel Corporation
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
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
#include <sys/stat.h>
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
#include <assert.h>
#include "diamond_consts.h"
#include "diamond_types.h"
#include "lib_tools.h"
#include "socket_trans.h"
#include "obj_attr.h"
#include "lib_odisk.h"
#include "lib_log.h"
#include "lib_dctl.h"
#include "lib_hstub.h"
#include "lib_auth.h"
#include "hstub_impl.h"


static char const cvsid[] =
    "$Header$";

void
hstub_read_log(sdevice_state_t * dev)
{
	conn_info_t    *cinfo;
	int             header_offset,
	                header_remain;
	int             data_offset,
	                data_remain;
	ssize_t         dsize;
	char           *data;
	char           *data_buf = NULL;

	cinfo = &dev->con_data;


	/*
	 * Find out where we are in the recieve state machine and
	 * set the header* and data* variables accordingly.
	 */

	if (cinfo->log_rx_state == LOG_RX_NO_PENDING) {
		header_remain = sizeof(cinfo->log_rx_header);
		header_offset = 0;
		data_remain = 0;
		data_offset = 0;

	} else if (cinfo->log_rx_state == LOG_RX_HEADER) {

		header_offset = cinfo->log_rx_offset;
		header_remain = sizeof(cinfo->log_rx_header) - header_offset;
		data_remain = 0;
		data_offset = 0;
	} else {
		assert(cinfo->log_rx_state == LOG_RX_DATA);
		header_remain = 0;
		header_offset = 0;
		data_offset = cinfo->log_rx_offset;
		data_remain = ntohl(cinfo->log_rx_header.log_len) -
		    data_offset;
		data_buf = cinfo->log_rx_data;
	}



	if (header_remain > 0) {
		data = (char *) &cinfo->log_rx_header;
		dsize = recv(cinfo->log_fd, (char *) &data[header_offset],
			     header_remain, 0);

		/*
		 * Handle some of the different error cases.
		 */
		if (dsize < 0) {
			/*
			 * The call failed, the only possibility is that
			 * we didn't have enough data for it.  In that
			 * case we return and retry later.
			 */
			if (errno == EAGAIN) {
				/*
				 * nothing has happened, so we should not
				 * need to update the state.
				 */
				return;
			} else {
			    	log_message(LOGT_NET, LOGL_CRIT,
			    	    "hstub_readlog: broken socket");
				hstub_conn_down(dev);
				return;
			}
		}


		/*
		 * Look at how much data we recieved, if we didn't get
		 * enough to complete the header, then we store
		 * the partial state and return, the next try will
		 * get rest of the header.
		 */
		if (dsize != header_remain) {
			cinfo->log_rx_offset = header_offset + dsize;
			cinfo->log_rx_state = LOG_RX_HEADER;
			return;
		}

		/*
		 * If we fall through here, then we have the full header,
		 * so we need to setup the parameters for reading the data
		 * portion.
		 */
		data_offset = 0;
		data_remain = ntohl(cinfo->log_rx_header.log_len);
		assert(ntohl(cinfo->log_rx_header.log_magic) ==
		       LOG_MAGIC_HEADER);

		/*
		 * get a buffer to store the data if appropriate 
		 */
		if (data_remain > 0) {
			data_buf = (char *) malloc(data_remain);
			if (data_buf == NULL) {
			    	log_message(LOGT_NET, LOGL_CRIT,
			    	    "hstub_readlog: malloc failed");
				hstub_conn_down(dev);
				return;
			}
		}
	}


	/*
	 * Now we try to get the remaining data associated with this
	 * command.
	 */
	if (data_remain > 0) {
		dsize = recv(cinfo->log_fd, &data_buf[data_offset],
			     data_remain, 0);

		if (dsize < 0) {

			/*
			 * The call failed, the only possibility is that
			 * we didn't have enough data for it.  In that
			 * case we return and retry later.
			 */
			if (errno == EAGAIN) {
				cinfo->log_rx_offset = data_offset;
				cinfo->log_rx_data = data_buf;
				cinfo->log_rx_state = LOG_RX_DATA;
				return;
			} else {
			    	log_message(LOGT_NET, LOGL_CRIT,
			    	    "hstub_readlog: malloc failed");
				hstub_conn_down(dev);
				return;
			}
		}


		/*
		 * Look at how much data we recieved, if we didn't get
		 * enough to complete the header, then we store
		 * the partial state and return, the next try will
		 * get rest of the header.
		 */
		if (dsize != data_remain) {
			cinfo->log_rx_offset = data_offset + dsize;
			cinfo->log_rx_data = data_buf;
			cinfo->log_rx_state = LOG_RX_DATA;
			return;
		}

	}

	cinfo->stat_log_rx++;
	cinfo->stat_log_byte_rx += sizeof(cinfo->log_rx_header) +
	    ntohl(cinfo->log_rx_header.log_len);

	/*
	 * If we get here we have the full log message, now
	 * call the function that handles it.  The called function will free
	 * the data when done.
	 */

	/*
	 * XXX get the correct devid 
	 */
	/*
	 * call callback function for the log 
	 */
	dsize = ntohl(cinfo->log_rx_header.log_len);
	(*dev->hstub_log_data_cb) (dev->hcookie, data_buf, dsize,
				   cinfo->dev_id);

	cinfo->log_rx_state = LOG_RX_NO_PENDING;
	return;
}

void
hstub_except_log(sdevice_state_t * dev)
{
	printf("except_log \n");
}


void
hstub_write_log(sdevice_state_t * dev)
{
	printf("write_log \n");
}
