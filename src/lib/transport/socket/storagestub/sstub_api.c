/*
 *      Diamond (Release 1.0)
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
#include "diamond_consts.h"
#include "diamond_types.h"
#include "lib_tools.h"
#include "obj_attr.h"
#include "lib_odisk.h"
#include "socket_trans.h"
#include "lib_dctl.h"
#include "lib_sstub.h"
#include "sstub_impl.h"


static char const cvsid[] =
    "$Header$";






/*
 * Send the current statistics on a search.  After this is 
 * called the stat buffer is no longer needed.
 */
int
sstub_send_stats(void *cookie, dev_stats_t * stats, int len)
{

	char           *buffer;
	int             data_size;
	int             num_filters;
	dstats_subheader_t *dhead;
	fstats_subheader_t *fhead;
	filter_stats_t *fstats;
	int             i;
	cstate_t       *cstate;
	control_header_t *cheader;
	int             err;

	cstate = (cstate_t *) cookie;

	/*
	 * get the number of filters and total size of the data portion 
	 */
	num_filters = stats->ds_num_filters;
	data_size = sizeof(*dhead) + (num_filters * sizeof(*fhead));

	/*
	 * allocate a buffer to hold the data 
	 */
	buffer = (char *) malloc(data_size);
	if (buffer == NULL) {
		return (ENOMEM);
	}

	/*
	 * allocated a control message header, for any failure
	 * we just return an error because failure to send this message
	 * need not be fatal.
	 */
	cheader = (control_header_t *) malloc(sizeof(*cheader));
	if (cheader == NULL) {
		/*
		 * XXX 
		 */
		printf("no memory for header \n");
		free(buffer);
		return (ENOMEM);
	}

	/*
	 * Fill in the control header for this
	 * message.
	 */

	cheader->generation_number = 0;	/* XXX ??? */
	cheader->command = htonl(CNTL_CMD_RET_STATS);
	cheader->data_len = htonl(data_size);
	cheader->spare = (uint32_t) buffer;

	/*
	 * Build the main statistics 
	 */
	dhead = (dstats_subheader_t *) buffer;

	dhead->dss_total_objs = htonl(stats->ds_objs_total);
	dhead->dss_objs_proc = htonl(stats->ds_objs_processed);
	dhead->dss_objs_drop = htonl(stats->ds_objs_dropped);
	dhead->dss_objs_bp = htonl(stats->ds_objs_nproc);
	dhead->dss_system_load = htonl(stats->ds_system_load);
	/*
	 * XXX 64 bit 
	 */
	dhead->dss_avg_obj_time = stats->ds_avg_obj_time;
	dhead->dss_num_filters = htonl(stats->ds_num_filters);


	/*
	 * For each of the filters fill in the associated
	 * statistics.
	 */

	fhead = (fstats_subheader_t *) & buffer[sizeof(*dhead)];

	for (i = 0; i < num_filters; i++) {
		fstats = &stats->ds_filter_stats[i];

		strncpy(fhead->fss_name, fstats->fs_name, MAX_FILTER_NAME);
		fhead->fss_name[MAX_FILTER_NAME - 1] = '\0';
		fhead->fss_objs_processed = ntohl(fstats->fs_objs_processed);
		fhead->fss_objs_dropped = ntohl(fstats->fs_objs_dropped);
		/*
		 * JIAYING: cache related info 
		 */
		fhead->fss_objs_cache_dropped =
		    ntohl(fstats->fs_objs_cache_dropped);
		fhead->fss_objs_cache_passed =
		    ntohl(fstats->fs_objs_cache_passed);
		fhead->fss_objs_compute = ntohl(fstats->fs_objs_compute);
		/*
		 * XXX 64 bit byte order below 
		 */
		/*
		 * XXX 64 bit byte order below 
		 */
		fhead->fss_avg_exec_time = fstats->fs_avg_exec_time;
		fhead++;
	}


	/*
	 * mark the control state that there is a pending
	 * control message to send.
	 */

	pthread_mutex_lock(&cstate->cmutex);
	cstate->flags |= CSTATE_CONTROL_DATA;

	err = ring_enq(cstate->control_tx_ring, (void *) cheader);
	pthread_mutex_unlock(&cstate->cmutex);
	if (err) {
		/*
		 * XXX 
		 */
		printf("can't enq message \n");
		exit(1);
	}

	return (0);
}

/*
 * XXX do we manage the complete ring also?? 
 */
float
sstub_get_drate(void *cookie)
{

	cstate_t       *cstate;
	cstate = (cstate_t *) cookie;

	return (ring_2drate(cstate->partial_obj_ring));
}


/*
 * Send an object. 
 *
 * return current queue depth??
 */
int
sstub_send_obj(void *cookie, obj_data_t * obj, int ver_no, int complete)
{

	cstate_t       *cstate;
	int             err;

	cstate = (cstate_t *) cookie;

	/*
	 * Set a flag to indicate there is object
	 * data associated with our connection.
	 */
	/*
	 * XXX log 
	 */
	pthread_mutex_lock(&cstate->cmutex);
	cstate->flags |= CSTATE_OBJ_DATA;
	if (complete) {
		err =
		    ring_2enq(cstate->complete_obj_ring, (void *) obj,
			      (void *) ver_no);
	} else {
		err =
		    ring_2enq(cstate->partial_obj_ring, (void *) obj,
			      (void *) ver_no);
	}
	pthread_mutex_unlock(&cstate->cmutex);

	if (err) {
		/*
		 * XXX log 
		 */
		/*
		 * XXX how do we handle this 
		 */
		return (err);
	}

	return (0);
}

int
sstub_get_partial(void *cookie, obj_data_t ** obj)
{

	cstate_t       *cstate;
	int             err;
	void           *vnum;

	cstate = (cstate_t *) cookie;

	/*
	 * Set a flag to indicate there is object
	 * data associated with our connection.
	 */
	/*
	 * XXX log 
	 */
	pthread_mutex_lock(&cstate->cmutex);
	err = ring_2deq(cstate->partial_obj_ring, (void **) obj,
			(void **) &vnum);
	pthread_mutex_unlock(&cstate->cmutex);

	return (err);
}

int
sstub_flush_objs(void *cookie, int ver_no)
{

	cstate_t       *cstate;
	int             err;
	obj_data_t     *obj;
	void           *junk;
	void           *vnum;
	listener_state_t *lstate;

	cstate = (cstate_t *) cookie;
	lstate = cstate->lstate;

	/*
	 * Set a flag to indicate there is object
	 * data associated with our connection.
	 */
	/*
	 * XXX log 
	 */
	while (1) {
		pthread_mutex_lock(&cstate->cmutex);
		err = ring_2deq(cstate->complete_obj_ring,
				(void **) &junk, (void **) &vnum);
		pthread_mutex_unlock(&cstate->cmutex);

		/*
		 * we got through them all 
		 */
		if (err) {
			break;
		}
		obj = (obj_data_t *) junk;
		(*lstate->release_obj_cb) (cstate->app_cookie, obj);
	}

	while (1) {
		pthread_mutex_lock(&cstate->cmutex);
		err = ring_2deq(cstate->partial_obj_ring,
				(void **) &junk, (void **) &vnum);
		pthread_mutex_unlock(&cstate->cmutex);

		/*
		 * we got through them all 
		 */
		if (err) {
			return (0);
		}
		obj = (obj_data_t *) junk;
		(*lstate->release_obj_cb) (cstate->app_cookie, obj);

	}

	return (0);
}


int
sstub_send_log(void *cookie, char *data, int len)
{

	cstate_t       *cstate;

	cstate = (cstate_t *) cookie;

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


	return (0);
}

int
sstub_send_dev_char(void *cookie, device_char_t * dev_char)
{

	cstate_t       *cstate;
	control_header_t *cheader;
	devchar_subhead_t *shead;
	int             err;

	cstate = (cstate_t *) cookie;


	cheader = (control_header_t *) malloc(sizeof(*cheader));
	if (cheader == NULL) {
		/*
		 * XXX 
		 */
		printf("no memory for header \n");
		exit(1);
	}

	shead = (devchar_subhead_t *) malloc(sizeof(*shead));
	if (shead == NULL) {
		/*
		 * XXX 
		 */
		printf("no memory for subheader \n");
		exit(1);
	}


	cheader->generation_number = 0;	/* XXX ??? */
	cheader->command = htonl(CNTL_CMD_RET_CHAR);
	cheader->data_len = htonl(sizeof(*shead));
	cheader->spare = (uint32_t) shead;

	shead->dcs_isa = htonl(dev_char->dc_isa);
	/*
	 * XXX bswap these 
	 */
	shead->dcs_speed = dev_char->dc_speed;
	shead->dcs_mem = dev_char->dc_mem;


	/*
	 * mark the control state that there is a pending
	 * control message to send.
	 */

	pthread_mutex_lock(&cstate->cmutex);
	cstate->flags |= CSTATE_CONTROL_DATA;

	err = ring_enq(cstate->control_tx_ring, (void *) cheader);
	pthread_mutex_unlock(&cstate->cmutex);
	if (err) {
		/*
		 * XXX 
		 */
		printf("can't enq message \n");
		exit(1);
	}


	return (0);
}



/*
 * This is the initialization function that is
 * called by the searchlet library when we startup.
 */

/*
 * XXX callback for new packets 
 */
void           *
sstub_init(sstub_cb_args_t * cb_args)
{
	return sstub_init_2(cb_args, 0);
}


/*
 * This is a new version of sstub_init which allows
 * for binding only to localhost.
 */
void *
sstub_init_2(sstub_cb_args_t * cb_args,
	     int bind_only_locally)
{
	listener_state_t *list_state;
	int             err;

	list_state = (listener_state_t *) malloc(sizeof(*list_state));
	if (list_state == NULL) {
		return (NULL);
	}

	/*
	 * clear out list state, this will also clear all the appropriate
	 * flags.
	 */
	memset((char *) list_state, 0, sizeof(*list_state));

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
	list_state->setlog_cb = cb_args->setlog_cb;
	list_state->rleaf_cb = cb_args->rleaf_cb;
	list_state->wleaf_cb = cb_args->wleaf_cb;
	list_state->lleaf_cb = cb_args->lleaf_cb;
	list_state->lnode_cb = cb_args->lnode_cb;
	list_state->sgid_cb = cb_args->sgid_cb;
	list_state->clear_gids_cb = cb_args->clear_gids_cb;
	list_state->set_blob_cb = cb_args->set_blob_cb;
	list_state->set_offload_cb = cb_args->set_offload_cb;

	/*
	 * Open the listner sockets for the different types of connections.
	 */
	err = sstub_new_sock(&list_state->control_fd, CONTROL_PORT,
			     bind_only_locally);
	if (err) {
		/*
		 * XXX log, 
		 */
		printf("failed to create control \n");
		free(list_state);
		return (NULL);
	}

	err = sstub_new_sock(&list_state->data_fd, DATA_PORT,
			     bind_only_locally);
	if (err) {
		/*
		 * XXX log, 
		 */
		printf("failed to create data \n");
		free(list_state);
		return (NULL);
	}

	err = sstub_new_sock(&list_state->log_fd, LOG_PORT,
			     bind_only_locally);
	if (err) {
		/*
		 * XXX log, 
		 */
		printf("failed to create log \n");
		free(list_state);
		return (NULL);
	}

	return ((void *) list_state);
}

int
sstub_wleaf_response(void *cookie, int err, int32_t opid)
{
	cstate_t       *cstate;
	control_header_t *cheader;
	dctl_subheader_t *shead;
	int             eno;
	int             tot_len;

	cstate = (cstate_t *) cookie;
	tot_len = sizeof(*shead);

	cheader = (control_header_t *) malloc(sizeof(*cheader));
	if (cheader == NULL) {
		/*
		 * XXX 
		 */
		printf("no memory for header \n");
		exit(1);
	}

	shead = (dctl_subheader_t *) malloc(tot_len);
	if (shead == NULL) {
		/*
		 * XXX 
		 */
		free(cheader);
		printf("no memory for subheader \n");
		exit(1);
	}


	cheader->generation_number = 0;	/* XXX ??? */
	cheader->command = htonl(CNTL_CMD_WLEAF_DONE);
	cheader->data_len = htonl(tot_len);
	cheader->spare = (uint32_t) shead;

	shead->dctl_err = htonl(err);
	shead->dctl_opid = htonl(opid);
	shead->dctl_plen = htonl(0);
	shead->dctl_dlen = htonl(0);

	/*
	 * mark the control state that there is a pending
	 * control message to send.
	 */

	pthread_mutex_lock(&cstate->cmutex);
	cstate->flags |= CSTATE_CONTROL_DATA;

	eno = ring_enq(cstate->control_tx_ring, (void *) cheader);
	pthread_mutex_unlock(&cstate->cmutex);
	if (eno) {
		/*
		 * XXX 
		 */
		free(shead);
		free(cheader);
		printf("can't enq message \n");
		exit(1);
	}

	return (0);
}


int
sstub_rleaf_response(void *cookie, int err, dctl_data_type_t dtype,
		     int len, char *data, int32_t opid)
{
	cstate_t       *cstate;
	control_header_t *cheader;
	dctl_subheader_t *shead;
	int             eno;
	int             tot_len;

	cstate = (cstate_t *) cookie;

	if (err == 0) {
		tot_len = sizeof(*shead) + len;
	} else {
		tot_len = sizeof(*shead);
	}

	cheader = (control_header_t *) malloc(sizeof(*cheader));
	if (cheader == NULL) {
		/*
		 * XXX 
		 */
		printf("no memory for header \n");
		exit(1);
	}

	shead = (dctl_subheader_t *) malloc(tot_len);
	if (shead == NULL) {
		/*
		 * XXX 
		 */
		free(cheader);
		printf("no memory for subheader \n");
		exit(1);
	}


	cheader->generation_number = 0;	/* XXX ??? */
	cheader->command = htonl(CNTL_CMD_RLEAF_DONE);
	cheader->data_len = htonl(tot_len);
	cheader->spare = (uint32_t) shead;

	shead->dctl_err = htonl(err);
	shead->dctl_opid = htonl(opid);
	shead->dctl_plen = htonl(0);
	shead->dctl_dtype = htonl(dtype);
	shead->dctl_dlen = htonl(len);

	if (err == 0) {
		memcpy(shead->dctl_data, data, len);
	}

	/*
	 * mark the control state that there is a pending
	 * control message to send.
	 */

	pthread_mutex_lock(&cstate->cmutex);
	cstate->flags |= CSTATE_CONTROL_DATA;

	eno = ring_enq(cstate->control_tx_ring, (void *) cheader);
	pthread_mutex_unlock(&cstate->cmutex);
	if (eno) {
		/*
		 * XXX 
		 */
		free(shead);
		free(cheader);
		printf("can't enq message \n");
		exit(1);
	}

	return (0);
}

int
sstub_lleaf_response(void *cookie, int err, int num_ents, dctl_entry_t * data,
		     int32_t opid)
{
	cstate_t       *cstate;
	control_header_t *cheader;
	dctl_subheader_t *shead;
	int             eno;
	int             tot_len;
	int             dlen;

	cstate = (cstate_t *) cookie;

	dlen = num_ents * sizeof(dctl_entry_t);

	if (err == 0) {
		tot_len = sizeof(*shead) + dlen;
	} else {
		if (err != ENOSPC) {
			dlen = 0;
		}
		tot_len = sizeof(*shead);
	}

	cheader = (control_header_t *) malloc(sizeof(*cheader));
	if (cheader == NULL) {
		/*
		 * XXX 
		 */
		printf("no memory for header \n");
		exit(1);
	}

	shead = (dctl_subheader_t *) malloc(tot_len);
	if (shead == NULL) {
		/*
		 * XXX 
		 */
		free(cheader);
		printf("no memory for subheader \n");
		exit(1);
	}


	cheader->generation_number = 0;	/* XXX ??? */
	cheader->command = htonl(CNTL_CMD_LLEAFS_DONE);
	cheader->data_len = htonl(tot_len);
	cheader->spare = (uint32_t) shead;

	shead->dctl_err = htonl(err);
	shead->dctl_opid = htonl(opid);
	shead->dctl_plen = htonl(0);
	shead->dctl_dlen = htonl(dlen);

	if (err == 0) {
		memcpy(shead->dctl_data, (char *) data, dlen);
	}

	/*
	 * mark the control state that there is a pending
	 * control message to send.
	 */

	pthread_mutex_lock(&cstate->cmutex);
	cstate->flags |= CSTATE_CONTROL_DATA;

	eno = ring_enq(cstate->control_tx_ring, (void *) cheader);
	pthread_mutex_unlock(&cstate->cmutex);
	if (eno) {
		/*
		 * XXX 
		 */
		free(shead);
		free(cheader);
		printf("can't enq message \n");
		exit(1);
	}
	return (0);

}

int
sstub_lnode_response(void *cookie, int err, int num_ents, dctl_entry_t * data,
		     int32_t opid)
{
	cstate_t       *cstate;
	control_header_t *cheader;
	dctl_subheader_t *shead;
	int             eno;
	int             tot_len;
	int             dlen;

	cstate = (cstate_t *) cookie;

	dlen = num_ents * sizeof(dctl_entry_t);

	if (err == 0) {
		tot_len = sizeof(*shead) + dlen;
	} else {
		if (err != ENOSPC) {
			dlen = 0;
		}
		tot_len = sizeof(*shead);
	}

	cheader = (control_header_t *) malloc(sizeof(*cheader));
	if (cheader == NULL) {
		/*
		 * XXX 
		 */
		printf("no memory for header \n");
		exit(1);
	}

	shead = (dctl_subheader_t *) malloc(tot_len);
	if (shead == NULL) {
		/*
		 * XXX 
		 */
		free(cheader);
		printf("no memory for subheader \n");
		exit(1);
	}


	cheader->generation_number = 0;	/* XXX ??? */
	cheader->command = htonl(CNTL_CMD_LNODES_DONE);
	cheader->data_len = htonl(tot_len);
	cheader->spare = (uint32_t) shead;

	shead->dctl_err = htonl(err);
	shead->dctl_opid = htonl(opid);
	shead->dctl_plen = htonl(0);
	shead->dctl_dlen = htonl(dlen);

	if (err == 0) {
		memcpy(shead->dctl_data, (char *) data, dlen);
	}

	/*
	 * mark the control state that there is a pending
	 * control message to send.
	 */

	pthread_mutex_lock(&cstate->cmutex);
	cstate->flags |= CSTATE_CONTROL_DATA;

	eno = ring_enq(cstate->control_tx_ring, (void *) cheader);
	pthread_mutex_unlock(&cstate->cmutex);
	if (eno) {
		/*
		 * XXX 
		 */
		free(shead);
		free(cheader);
		printf("can't enq message \n");
		exit(1);
	}
	return (0);
}
