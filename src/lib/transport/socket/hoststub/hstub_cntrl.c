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
#include <string.h>
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
 * Take the device characteristics we recieved and
 * store them as part of the device state.  We will use
 * these to answer requests.  The freshness will be determined
 * by how often we go an make the requests.
 */

static void
store_dev_char(sdevice_state_t *dev, char *data_buf)
{
	devchar_subhead_t *	shead;

	shead = (devchar_subhead_t *)data_buf;

	dev->dev_char.dc_isa = ntohl(shead->dcs_isa);
	/* XXX byte swap when we get it working */
	dev->dev_char.dc_speed = shead->dcs_isa;
	dev->dev_char.dc_mem = shead->dcs_mem;
	dev->dev_char.dc_devid = dev->con_data.dev_id;
}



/*
 * This stores caches the statistics to answer requests
 * from users.
 */
static void
store_dev_stats(sdevice_state_t *dev, char *data_buf)
{
	dstats_subheader_t *	shead;
	fstats_subheader_t *	fhead;
	dev_stats_t *		dstats;
	int			len;
	int			num_filt;
	int			offset;
	int			i;

	shead = (dstats_subheader_t *)data_buf;

	num_filt = ntohl(shead->dss_num_filters);
	len = DEV_STATS_SIZE(num_filt);


	if (len > dev->stat_size) {
		if (dev->dstats != NULL) {
			free(dev->dstats);
		}

		dstats = (dev_stats_t *) malloc(len);
		assert(dstats != NULL);
		dev->dstats = dstats;

		dev->stat_size = len;

	} else {
		dstats = dev->dstats;
		dev->stat_size  = len;
	}


	dstats->ds_objs_total = ntohl(shead->dss_total_objs);
	dstats->ds_objs_processed = ntohl(shead->dss_objs_proc);
	dstats->ds_objs_dropped = ntohl(shead->dss_objs_drop);
	dstats->ds_system_load = ntohl(shead->dss_system_load);
	/* XXX 64 bit ntohl */
	dstats->ds_avg_obj_time = shead->dss_avg_obj_time;
	dstats->ds_num_filters = ntohl(shead->dss_num_filters);

	for (i = 0; i < num_filt; i ++) {
		offset = sizeof(*shead) + (i * sizeof(*fhead));
		fhead = (fstats_subheader_t *)&data_buf[offset];

		strncpy(dstats->ds_filter_stats[i].fs_name,
				fhead->fss_name, MAX_FILTER_NAME);
		dstats->ds_filter_stats[i].fs_name[MAX_FILTER_NAME-1] =
				'\0';
	
		dstats->ds_filter_stats[i].fs_objs_processed = 
				ntohl(fhead->fss_objs_processed);
			
		dstats->ds_filter_stats[i].fs_objs_dropped = 
				ntohl(fhead->fss_objs_dropped);

		/* XXX byte order !!! */
		dstats->ds_filter_stats[i].fs_avg_exec_time = 
				fhead->fss_avg_exec_time;
	}
}

static void
process_control(sdevice_state_t *dev, conn_info_t *cinfo, char *data_buf)
{

	uint32_t	cmd = ntohl(cinfo->control_rx_header.command);

	switch(cmd) {

		case CNTL_CMD_RET_STATS:
			store_dev_stats(dev, data_buf);
			free(data_buf);
			break;


		case CNTL_CMD_RET_CHAR:
			store_dev_char(dev, data_buf);
			free(data_buf);
			break;

		default:
			printf("XXX process control: unknown command %d \n",
					cmd);
			exit(1);

	}

}





void
hstub_read_cntrl(sdevice_state_t  *dev)
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

	if (cinfo->control_rx_state == CONTROL_RX_NO_PENDING) {
		header_remain = sizeof(cinfo->control_rx_header);
		header_offset = 0;
		data_remain = 0;
		data_offset = 0;

	} else if (cinfo->control_rx_state == CONTROL_RX_HEADER) {

		header_offset = cinfo->control_rx_offset;
		header_remain = sizeof(cinfo->control_rx_header) -
		       	header_offset;
		data_remain = 0;
		data_offset = 0;
	} else {
		assert(cinfo->control_rx_state == CONTROL_RX_DATA);
		header_remain = 0;
		header_offset = 0;
		data_offset = cinfo->control_rx_offset;
		data_remain = ntohl(cinfo->control_rx_header.data_len) -
			data_offset;
		data_buf = cinfo->control_rx_data;
	}



	if (header_remain > 0) {
		data = (char *)&cinfo->control_rx_header;
		dsize = recv(cinfo->control_fd, (char *)&data[header_offset],
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
			cinfo->control_rx_offset = header_offset + dsize;
			cinfo->control_rx_state = CONTROL_RX_HEADER;
			return;
		}

		/*
		 * If we fall through here, then we have the full header,
		 * so we need to setup the parameters for reading the data
		 * portion.
		 */
		data_offset = 0;
		data_remain = ntohl(cinfo->control_rx_header.data_len);

		/* get a buffer to store the data if appropriate */
		if (data_remain > 0) {
			data_buf = (char *) malloc(data_remain);
			if (data_buf == NULL) {
				/* XXX crap, how do I get out of this */
				exit(1);
			}
		}
	}


	/*
	 * Now we try to get the remaining data associated with this
	 * command.
	 */
	if (data_remain > 0) {
		assert(data_buf != NULL);

		dsize = recv(cinfo->control_fd, &data_buf[data_offset], 
				data_remain, 0);

		if (dsize < 0) {
		
			/*
			 * The call failed, the only possibility is that
			 * we didn't have enough data for it.  In that
			 * case we return and retry later.
			 */
			if (errno == EAGAIN) {
				cinfo->control_rx_offset = data_offset;
				cinfo->control_rx_data = data_buf;
				cinfo->control_rx_state = CONTROL_RX_DATA;
				return;
			} else {
				/*
				 * some un-handled error happened, we
				 * just shutdown the connection.
				 */	
				/* XXX log */
				perror("process_control");
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
			cinfo->control_rx_offset = data_offset + dsize;
			cinfo->control_rx_data = data_buf;
			cinfo->control_rx_state = CONTROL_RX_DATA;
			return;
		}

	}

	/*
	 * If we get here we have the full control message, now
	 * call the function that handles it.  The called function will free
	 * the data when done.
	 */

	process_control(dev, cinfo, data_buf);
	
	cinfo->control_rx_state = CONTROL_RX_NO_PENDING;
	return;
}

void
hstub_except_cntrl(sdevice_state_t *dev)
{
	printf("hstub_except_control \n");	
}


/*
 * Send a control command.
 */
void
hstub_write_cntrl(sdevice_state_t *dev)
{
	char *			data;
	size_t			send_len;
	conn_info_t *		cinfo;
	control_header_t *	cheader;
	int			remain_header;
	int			header_offset;
	int			remain_data;
	int			data_offset;


	cinfo = &dev->con_data;


	if (cinfo->control_state == CONTROL_TX_NO_PENDING) {

		/*
		 * See if there is a control message to process.
		 */
		cheader = (control_header_t *)ring_deq(dev->device_ops);
		if (cheader == NULL) {
			pthread_mutex_lock(&cinfo->mutex);
			cinfo->flags &= ~CINFO_PENDING_CONTROL;
			pthread_mutex_unlock(&cinfo->mutex);
			return;
		}
	
		remain_header = sizeof(*cheader);
		header_offset = 0;
		data_offset = 0;
		remain_data = ntohl(cheader->data_len);

	} else if (cinfo->control_state == CONTROL_TX_HEADER) {
		cheader = cinfo->control_header;

		header_offset = cinfo->control_offset;;
		remain_header = sizeof(*cheader) - header_offset;
		data_offset = 0;
		remain_data = ntohl(cheader->data_len);

	} else {
		assert(cinfo->control_state == CONTROL_TX_DATA);

		header_offset = 0;
		cheader = cinfo->control_header;
		remain_header = 0;
		data_offset = cinfo->control_offset;
		remain_data = ntohl(cheader->data_len) - data_offset;
	}
	

	if (remain_header != 0) {
		data = (char *)cheader;

		send_len = send(cinfo->control_fd, &data[header_offset], 
				remain_header, 0);
		if (send_len < 0) {
			if (errno == EAGAIN) {
				cinfo->control_header = cheader;
				cinfo->control_offset = header_offset;
				cinfo->control_state = CONTROL_TX_HEADER;
				return;
			} else {
				/* XXX log, what else do we do ?? */
				assert(0);
				return;
			}
		} 
	
		if (send_len != remain_header) {
			cinfo->control_header = cheader;
			cinfo->control_offset = header_offset + send_len;
			cinfo->control_state = CONTROL_TX_HEADER;
			return;
		}
	}

	if (remain_data != 0) {
		data = (char *)cheader->spare;
		send_len = send(dev->con_data.control_fd, &data[data_offset], 
				remain_data, 0);

		if (send_len < 0) {
			if (errno == EAGAIN) {
				cinfo->control_header = cheader;
				cinfo->control_offset = data_offset;
				cinfo->control_state = CONTROL_TX_DATA;
				return;
			} else {
				/* XXX log, what else do we do ?? */
				assert(0);
				return;
			}
		}

		if (send_len != remain_data) {
			cinfo->control_header = cheader;
			cinfo->control_offset = data_offset + send_len;
			cinfo->control_state = CONTROL_TX_DATA;
			return;
		}
		free(data);
	}

	cinfo->control_state = CONTROL_TX_NO_PENDING;
	free(cheader);
	return;
}

