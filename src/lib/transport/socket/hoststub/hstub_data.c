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
#include "lib_odisk.h"
#include "lib_hstub.h"
#include "hstub_impl.h"


/*
 * This is called when there is data waiting on
 * the object socket.
 */
void
hstub_read_data(sdevice_state_t *dev)
{
	obj_data_t *	obj;
	conn_info_t *	cinfo;
	int		header_offset, header_remain;
	int		attr_offset, attr_remain;
	int		data_offset, data_remain;
	int		rsize;
	uint32_t	alen, dlen;
	int		ver_no;
	char *		adata;
	char *		odata;
	char *		data;

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
		attr_remain = cinfo->data_rx_obj->attr_info.attr_len - 
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
		data = (char *)&cinfo->data_rx_header;

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
				/* XXX what to do?? */
				/* XXX log */
				perror("get_obj_data: unknown err: \n");
				exit(1);
			}
		}

		/* XXX look for zero for shutdown connections */

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
			/* XXX log */
			printf("get_obj_data:  bad magic number \n");
			exit(1);
		}

		/*
		 * Extract lengths of the two fields from the header.
		 */
		alen = ntohl(cinfo->data_rx_header.attr_len);
		dlen = ntohl(cinfo->data_rx_header.data_len);


		/* try to allocate storage for the attributes */
		if (alen > 0) {
			adata = (char *) malloc(alen);
			if (adata == NULL) {
				/* XXX treate as partial and recover later ??*/
				printf("failed to allocation attribute data \n");
				exit(1);
		
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
				/* XXX treate as partial and recover later?? */
				printf("failed to allocation object data \n");
				exit(1);
			}
		} else {
			odata = NULL;
		}


		/*
	 	 * allocate an obj_data_t structure to hold the object
		 * and populate it.
		 */
	
		obj = (obj_data_t *)malloc(sizeof(*obj));
		if (obj == NULL) {
			printf("XXX crap, no space for object data \n");
			exit(1);
		}
	
		obj->data_len = dlen;
		obj->data = odata;
		obj->attr_info.attr_len = alen;
		obj->attr_info.attr_data = adata;

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
		data = cinfo->data_rx_obj->attr_info.attr_data;

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
				/* XXX what to do?? */
				/* XXX log */
				perror("get_obj_data: err reading attrs\n");
				exit(1);
			}
		}

		if (rsize != attr_remain) {
			/* XXX save partial results */
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
				/* XXX what to do?? */
				/* XXX log */
				perror("get_obj_data: err reading data\n");
				exit(1);
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
	
	cinfo->data_rx_state = DATA_RX_NO_PENDING;
	ver_no = ntohl(cinfo->data_rx_header.version_num);
	/* XXX extract version number */

	/*
	 * Now sent the callback.  Should there be a return ??
	 */
	(*dev->hstub_new_obj_cb)(dev->hcookie, cinfo->data_rx_obj, ver_no);
}

void
hstub_except_data(sdevice_state_t *dev)
{
	printf("except_data \n");	
}

	
void
hstub_write_data(sdevice_state_t * dev)
{
	printf("write_data \n");	
}



