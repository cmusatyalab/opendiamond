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
#include "ring.h"
#include "lib_searchlet.h"
#include "socket_trans.h"
#include "obj_attr.h"
#include "lib_od.h"
#include "lib_odisk.h"
#include "lib_dctl.h"
#include "lib_hstub.h"
#include "hstub_impl.h"



void
hstub_read_log(sdevice_state_t  *dev)
{
	conn_info_t *	cinfo;
	int		header_offset, header_remain;
	int		data_offset, data_remain;
	int		dsize;
	char *		data;
	char *		data_buf = NULL;

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
		header_remain = sizeof(cinfo->log_rx_header) -
		       	header_offset;
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
		data = (char *)&cinfo->log_rx_header;
		dsize = recv(cinfo->log_fd, (char *)&data[header_offset],
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
				/*
				 * some un-handled error happened, 
				 */
				/* XXX what now */
				perror("uknown socket problem:");
				exit(1);
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
		assert( ntohl(cinfo->log_rx_header.log_magic) == 
					LOG_MAGIC_HEADER);

		/* get a buffer to store the data if appropriate */
		if (data_remain > 0) {
			data_buf = (char *) malloc(data_remain);
			if (data_buf == NULL) {
				/* XXX crap, how do I get out of this */
				printf("XXX failed to alloc log buffer \n");
				exit(1);
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
				/*
				 * some un-handled error happened, we
				 * just shutdown the connection.
				 */	
				/* XXX log */
				perror("process_log");
				exit(1);
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

    cinfo->stat_log_rx ++;
    cinfo->stat_log_byte_rx += sizeof(cinfo->log_rx_header) +
		    ntohl(cinfo->log_rx_header.log_len);

	/*
	 * If we get here we have the full log message, now
	 * call the function that handles it.  The called function will free
	 * the data when done.
	 */

	/* XXX get the correct devid */
	/* call callback function for the log */
	dsize = ntohl(cinfo->log_rx_header.log_len);
	(*dev->hstub_log_data_cb)(dev->hcookie, data_buf, dsize, cinfo->dev_id);

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


