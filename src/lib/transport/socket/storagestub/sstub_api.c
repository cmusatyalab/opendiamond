/*
 * These file handles a lot of the device specific code.  For the current
 * version we have state for each of the devices.
 */
#include <pthread.h>
#include <sys/types.h>
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
#include <string.h>
#include <assert.h>
#include "ring.h"
#include "obj_attr.h"
#include "lib_odisk.h"
#include "lib_searchlet.h"
#include "socket_trans.h"
#include "lib_sstub.h"
#include "sstub_impl.h"








/*
 * Send the current statistics on a search.
 *
 */
int
sstub_send_stats(void *cookie, dev_stats_t *stats, int len)
{

	char * 	buffer;
	int	data_size;
	int	num_filters;
	dstats_subheader_t	*dhead;
	fstats_subheader_t	*fhead;
	filter_stats_t		*fstats;
	int			i;
	cstate_t *		cstate;
	control_header_t *	cheader;
	int			err;

	cstate = (cstate_t *)cookie;

	/* get the number of filters and total size of the data portion */
	num_filters = stats->ds_num_filters;
	data_size = sizeof(*dhead) + (num_filters * sizeof(*fhead));

	/* allocate a buffer to hold the data */
	buffer = (char *)malloc(data_size);
	if (buffer == NULL) {
		return(ENOMEM);
	}

	/*
	 * allocated a control message header, for any failure
	 * we just return an error because failure to send this message
	 * need not be fatal.
	 */
	cheader = (control_header_t *)malloc(sizeof(*cheader));
	if (cheader == NULL) {
		/* XXX */
		printf("no memory for header \n");
		free(buffer);
		return(ENOMEM);
	}

	/*
	 * Fill in the control header for this
	 * message.
	 */

	cheader->generation_number = 0; /* XXX ??? */
	cheader->command = htonl(CNTL_CMD_RET_STATS);
	cheader->data_len = htonl(data_size);
	cheader->spare = (uint32_t)buffer;

	/*
	 * Build the main statistics 
	 */
	dhead = (dstats_subheader_t *)buffer;
	
	dhead->dss_total_objs = htonl(stats->ds_objs_total);
	dhead->dss_objs_proc = htonl(stats->ds_objs_processed);
	dhead->dss_objs_drop = htonl(stats->ds_objs_dropped);
	dhead->dss_system_load = htonl(stats->ds_system_load);
	/* XXX 64 bit */
	dhead->dss_avg_obj_time = stats->ds_avg_obj_time;
	dhead->dss_num_filters = htonl(stats->ds_num_filters);


	/*
	 * For each of the filters fill in the associated
	 * statistics.
	 */

	fhead = (fstats_subheader_t *) &buffer[sizeof(*dhead)];

	for (i=0; i < num_filters; i++) {
		fstats = &stats->ds_filter_stats[i];

		strncpy(fhead->fss_name, fstats->fs_name, MAX_FILTER_NAME);
		fhead->fss_name[MAX_FILTER_NAME-1] = '\0';
		fhead->fss_objs_processed = ntohl(fstats->fs_objs_processed);
		fhead->fss_objs_dropped = ntohl(fstats->fs_objs_dropped);
		/* XXX 64 bit byte order below */
		fhead->fss_avg_exec_time = fstats->fs_avg_exec_time;
		fhead++;
	}


	/*
	 * mark the control state that there is a pending
	 * control message to send.
	 */

	pthread_mutex_lock(&cstate->cmutex);
	cstate->flags |= CSTATE_CONTROL_DATA;

	err = ring_enq(cstate->control_tx_ring, (void *)cheader);
	pthread_mutex_unlock(&cstate->cmutex);
	if (err) {
		/* XXX */
		printf("can't enq message \n");
		exit(1);
	}

	return(0);
}



/*
 * Send an object. 
 *
 * return current queue depth??
 */
int
sstub_send_obj(void *cookie, obj_data_t *obj, int ver_no)
{

	cstate_t *	cstate;
	int		err;

	cstate = (cstate_t *)cookie;

	/*
	 * Set a flag to indicate there is object
	 * data associated with our connection.
	 */
	/* XXX log */
	pthread_mutex_lock(&cstate->cmutex);
	cstate->flags |= CSTATE_OBJ_DATA;
	err = ring_2enq(cstate->obj_ring, (void *)obj, (void *)ver_no);
	pthread_mutex_unlock(&cstate->cmutex);

	if (err) {
		/* XXX log */
		/* XXX how do we handle this */
		return(err);
	}

	return(0);
}

int
sstub_send_log(void *cookie, char *data, int len)
{

	cstate_t *	cstate;

	cstate = (cstate_t *)cookie;

	/*
	 * we not have any other send logs outstanding
	 * at this point.  
	 */
	assert(cstate->log_tx_buf == NULL);
	assert(len > 0);
	assert(data != NULL);

	pthread_mutex_lock(&cstate->cmutex);

	cstate->log_tx_buf = data;
	cstate->log_tx_len = len;
	cstate->log_tx_offset = 0;

	/*
	 * Set a flag to indicate there is object
	 * data associated with our connection.
	 */
	cstate->flags |= CSTATE_LOG_DATA;
	pthread_mutex_unlock(&cstate->cmutex);


	return(0);
}

int
sstub_send_dev_char(void *cookie, device_char_t *dev_char)
{

	cstate_t *		cstate;
	control_header_t *	cheader;
	devchar_subhead_t *	shead;
	int			err;

	cstate = (cstate_t *)cookie;


	cheader = (control_header_t *)malloc(sizeof(*cheader));
	if(cheader == NULL) {
		/* XXX */
		printf("no memory for header \n");
		exit(1);
	}

	shead = (devchar_subhead_t *)malloc(sizeof(*shead));
	if(shead == NULL) {
		/* XXX */
		printf("no memory for subheader \n");
		exit(1);
	}


	cheader->generation_number = 0; /* XXX ??? */
	cheader->command = htonl(CNTL_CMD_RET_CHAR);
	cheader->data_len = htonl(sizeof(*shead));
	cheader->spare = (uint32_t)shead;
	
	shead->dcs_isa = htonl(dev_char->dc_isa);
	/* XXX bswap these */
	shead->dcs_speed = dev_char->dc_speed;
	shead->dcs_mem = dev_char->dc_mem;


	/*
	 * mark the control state that there is a pending
	 * control message to send.
	 */

	pthread_mutex_lock(&cstate->cmutex);
	cstate->flags |= CSTATE_CONTROL_DATA;

	err = ring_enq(cstate->control_tx_ring, (void *)cheader);
	pthread_mutex_unlock(&cstate->cmutex);
	if (err) {
		/* XXX */
		printf("can't enq message \n");
		exit(1);
	}


	return(0);
}



/*
 * This is the initialization function that is
 * called by the searchlet library when we startup.
 */

/* XXX callback for new packets  */
void *
sstub_init(sstub_cb_args_t *cb_args)
{
	listener_state_t *	list_state;
	int			err;

	list_state = (listener_state_t *) malloc(sizeof(*list_state));	
	if (list_state == NULL) {
		return (NULL);
	}

	/*
	 * clear out list state, this will also clear all the appropriate
	 * flags.
	 */
	memset((char *)list_state, 0, sizeof(*list_state));

	/*
	 * Save all the callback functions.
	 */
	list_state->new_conn_cb = cb_args->new_conn_cb;
	list_state->close_conn_cb = cb_args->close_conn_cb;
	list_state->start_cb = cb_args->start_cb;
	list_state->stop_cb = cb_args->stop_cb;
	list_state->set_searchlet_cb = cb_args->set_searchlet_cb;
	list_state->set_list_cb = cb_args->set_list_cb;
	list_state->terminate_cb = cb_args->terminate_cb;
	list_state->release_obj_cb = cb_args->release_obj_cb;
	list_state->get_char_cb = cb_args->get_char_cb;
	list_state->get_stats_cb = cb_args->get_stats_cb;
	list_state->log_done_cb = cb_args->log_done_cb;

	/*
	 * Open the listner sockets for the different types of connections.
	 */
	err = sstub_new_sock(&list_state->control_fd, CONTROL_PORT);
	if (err) {
		/* XXX log,  */
		printf("failed to create control \n");
		free(list_state);
		return(NULL);
	}

	err = sstub_new_sock(&list_state->data_fd, DATA_PORT);
	if (err) {
		/* XXX log,  */
		printf("failed to create data \n");
		free(list_state);
		return(NULL);
	}

	err = sstub_new_sock(&list_state->log_fd, LOG_PORT);
	if (err) {
		/* XXX log,  */
		printf("failed to create log \n");
		free(list_state);
		return(NULL);
	}

	return((void *)list_state);
}


