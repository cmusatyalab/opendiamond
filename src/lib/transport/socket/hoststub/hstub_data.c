/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 3
 *
 *  Copyright (c) 2002-2007 Intel Corporation
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
 *  Copyright (c) 2006 Carnegie Mellon University
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
#include "lib_log.h"
#include "lib_odisk.h"
#include "lib_dctl.h"
#include "lib_hstub.h"
#include "lib_auth.h"
#include "hstub_impl.h"


static char const cvsid[] =
    "$Header$";

/*
 * This is called when there is data waiting on
 * the object socket.
 */
void
hstub_read_data(sdevice_state_t * dev)
{
	obj_data_t     *obj;
	obj_adata_t    *attr_data;
	conn_info_t    *cinfo;
	int             header_offset,
	                header_remain;
	int             attr_offset,
	                attr_remain;
	int             data_offset,
	                data_remain;
	ssize_t         rsize;
	uint32_t        alen,
	                dlen;
	int             ver_no;
	char           *adata;
	char           *odata;
	char           *data;
	int             err;

	cinfo = &dev->con_data;

	/*
	 * Look at the current state, if we have partial recieved
	 * some data then continue geting the object, otherwise
	 * start a new object.
	 */

	if (cinfo->data_rx_state == DATA_RX_NO_PENDING) {
		header_offset = 0;
		header_remain = sizeof(obj_header_t);
		attr_offset = 0;
		attr_remain = 0;
		data_offset = 0;
		data_remain = 0;

	} else if (cinfo->data_rx_state == DATA_RX_HEADER) {
		header_offset = cinfo->data_rx_offset;
		header_remain = sizeof(obj_header_t) - header_offset;
		attr_offset = 0;
		attr_remain = 0;
		data_offset = 0;
		data_remain = 0;


	} else if (cinfo->data_rx_state == DATA_RX_ATTR) {
		header_offset = 0;
		header_remain = 0;
		attr_offset = cinfo->data_rx_offset;
		attr_remain =
		    cinfo->data_rx_obj->attr_info.attr_dlist->adata_len -
		    attr_offset;
		data_offset = 0;
		data_remain = cinfo->data_rx_obj->data_len;

	} else {
		assert(cinfo->data_rx_state == DATA_RX_DATA);

		header_offset = 0;
		header_remain = 0;
		attr_offset = 0;
		attr_remain = 0;
		data_offset = cinfo->data_rx_offset;
		data_remain = cinfo->data_rx_obj->data_len - data_offset;
	}



	/*
	 * if there is still some remaining header data, then try
	 * to finish the remainder.
	 */
	if (header_remain > 0) {
		data = (char *) &cinfo->data_rx_header;

		rsize = recv(cinfo->data_fd, &data[header_offset],
			     header_remain, 0);
		if (rsize < 0) {
			if (errno == EAGAIN) {
				/*
				 * We don't have any data to read just now, 
				 * This probably should not happen.
				 */
				cinfo->data_rx_state = DATA_RX_HEADER;
				cinfo->data_rx_offset = header_offset;
				return;
			} else {
			    	log_message(LOGT_NET, LOGL_CRIT,
			    	    "hstub_read_data: broken socket");
				hstub_conn_down(dev);
				return;
			}
		}

		/*
		 * XXX look for zero for shutdown connections 
		 */

		if (rsize != header_remain) {
			cinfo->data_rx_state = DATA_RX_HEADER;
			cinfo->data_rx_offset = header_offset + rsize;
			return;
		}


		/*
		 * Okay, if we get here, we have the complete object header 
		 * from the network.  Now we parse it and allocate the other
		 * data structures that we are going to need to use
		 * the pull in the rest of the data.
		 */
		if (ntohl(cinfo->data_rx_header.obj_magic)
		    != OBJ_MAGIC_HEADER) {
			log_message(LOGT_NET, LOGL_CRIT,
			    "hstub_read_data: bad magic");
			hstub_conn_down(dev);
			return;
		}

		/*
		 * Extract lengths of the two fields from the header.
		 */
		alen = ntohl(cinfo->data_rx_header.attr_len);
		dlen = ntohl(cinfo->data_rx_header.data_len);


		/*
		 * try to allocate storage for the attributes 
		 */
		if (alen > 0) {
			adata = (char *) malloc(alen);
			if (adata == NULL) {
				log_message(LOGT_NET, LOGL_CRIT,
			    	    "hstub_read_data: malloc failed");
				hstub_conn_down(dev);
				return;
			}
		} else {
			adata = NULL;
		}

		/*
		 * Allocate storage for the data attributes.
		 */
		if (dlen > 0) {
			odata = (char *) malloc(dlen);
			if (odata == NULL) {
				log_message(LOGT_NET, LOGL_CRIT,
			    	    "hstub_read_data: malloc failed");
				hstub_conn_down(dev);
				return;
			}
		} else {
			odata = NULL;
		}


		/*
		 * allocate an obj_data_t structure to hold the object
		 * and populate it.
		 */

		obj = (obj_data_t *) malloc(sizeof(*obj));
		if (obj == NULL) {
			log_message(LOGT_NET, LOGL_CRIT,
			    "hstub_read_data: malloc failed");
			hstub_conn_down(dev);
			return;
		}
		obj->data_len = dlen;
		obj->data = odata;
		obj->base = odata;

		attr_data = (obj_adata_t *) malloc(sizeof(*attr_data));
		attr_data->adata_data = adata;
		attr_data->adata_base = adata;
		attr_data->adata_len = alen;
		attr_data->adata_next = NULL;

		obj->attr_info.attr_ndata = 1;
		obj->attr_info.attr_dlist = attr_data;
		obj->remain_compute =
		    (float) ntohl(cinfo->data_rx_header.remain_compute) /
		    1000.0;
		cinfo->data_rx_obj = obj;

		attr_offset = 0;
		attr_remain = alen;
		data_offset = 0;
		data_remain = dlen;
	}


	/*
	 * If there is attribute data, then get it .
	 */
	if (attr_remain > 0) {
		data = cinfo->data_rx_obj->attr_info.attr_dlist->adata_data;

		rsize = recv(cinfo->data_fd, &data[attr_offset],
			     attr_remain, 0);
		if (rsize < 0) {
			if (errno == EAGAIN) {
				/*
				 * We don't have enough data, so we 
				 * need to recover
				 * by saving the partial state and returning.
				 */
				cinfo->data_rx_state = DATA_RX_ATTR;
				cinfo->data_rx_offset = attr_offset;
				return;
			} else {
				log_message(LOGT_NET, LOGL_CRIT,
			    	    "hstub_read_data: socket down");
				hstub_conn_down(dev);
				return;
			}
		}

		if (rsize != attr_remain) {
			/*
			 * XXX save partial results 
			 */
			cinfo->data_rx_state = DATA_RX_ATTR;
			cinfo->data_rx_offset = attr_offset + rsize;
			return;
		}
	}



	/*
	 * If we got here, we have the attribute data, now we need
	 * to try and get the payload of the object.
	 */
	if (data_remain > 0) {
		data = cinfo->data_rx_obj->data;

		rsize = recv(cinfo->data_fd, &data[data_offset],
			     data_remain, 0);
		if (rsize < 0) {
			if (errno == EAGAIN) {
				/*
				 * We don't have enough data, so we need to 
				 * recover  by saving the partial state 
				 * and returning.
				 */
				cinfo->data_rx_state = DATA_RX_DATA;
				cinfo->data_rx_offset = data_offset;
				return;
			} else {
				log_message(LOGT_NET, LOGL_CRIT,
			    	    "hstub_read_data: socket down");
				hstub_conn_down(dev);
				return;
			}
		}

		if (rsize != data_remain) {
			/*
			 * we got some data but not all we need,  so
			 * we update our state machine and return.
			 */

			cinfo->data_rx_state = DATA_RX_DATA;
			cinfo->data_rx_offset = data_offset + rsize;
			return;
		}
	}

	cinfo->stat_obj_rx++;
	cinfo->stat_obj_attr_byte_rx +=
	    cinfo->data_rx_obj->attr_info.attr_dlist->adata_len;
	cinfo->stat_obj_hdr_byte_rx += sizeof(obj_header_t);
	cinfo->stat_obj_data_byte_rx += cinfo->data_rx_obj->data_len;
	cinfo->stat_obj_total_byte_rx +=
	    cinfo->data_rx_obj->attr_info.attr_dlist->adata_len +
	    sizeof(obj_header_t) + cinfo->data_rx_obj->data_len;



	cinfo->data_rx_state = DATA_RX_NO_PENDING;
	ver_no = ntohl(cinfo->data_rx_header.version_num);

	if ((cinfo->data_rx_obj->data_len == 0) &&
	    (cinfo->data_rx_obj->attr_info.attr_dlist->adata_len == 0)) {
		(*dev->hstub_search_done_cb) (dev->hcookie, ver_no);
		free(cinfo->data_rx_obj);

	} else {

		/*
		 * XXX put it into the object ring 
		 */
		obj_info_t     *oinfo;

		oinfo = (obj_info_t *) malloc(sizeof(*oinfo));
		assert(oinfo != NULL);

		oinfo->ver_num = ver_no;
		oinfo->obj = cinfo->data_rx_obj;

		err = ring_enq(dev->obj_ring, oinfo);
		assert(err == 0);
		dev->con_data.flags |= CINFO_PENDING_CREDIT;
	}
}

void
hstub_except_data(sdevice_state_t * dev)
{
	printf("except_data \n");
}


/*
 * This is called when we want to write the credit
 * count onto the data channel.
 */

void
hstub_write_data(sdevice_state_t * dev)
{
	conn_info_t    *cinfo;
	char           *data;
	ssize_t          send_size, mcount;
	int             count;

	cinfo = &dev->con_data;

	/*
	 * the only data we should ever need to write is 
	 * credit count messages.
	 */

	if ((cinfo->flags & CINFO_PENDING_CREDIT) == 0) {
		return;
	}

	/*
	 * build the credit count messages using the current state 
	 */
	cinfo->cc_msg.cc_magic = htonl(CC_MAGIC_HEADER);

	count = cinfo->obj_limit - ring_count(dev->obj_ring);
	if (count < 0) {
		count = 0;
	}
	cinfo->cc_msg.cc_count = htonl(count);

	/*
	 * send the messages 
	 */
	data = (char *) &cinfo->cc_msg;
	mcount = sizeof(credit_count_msg_t);
	while (mcount > 0) {
		send_size = send(cinfo->data_fd, data, mcount, 0);
		if (send_size == -1) {
			if (errno == EAGAIN) {
				continue;
			} else {
				perror("hstub_write_data");
				return;
			}
		}
		mcount -= send_size;
		data += send_size;
	}

	/*
	 * if successful, clear the flag 
	 */
	cinfo->flags &= ~CINFO_PENDING_CREDIT;
}
