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
#include <string.h>
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
#include "dctl_common.h"
#include "lib_hstub.h"
#include "hstub_impl.h"

/* XXX move to common header */
#define	HSTUB_RING_SIZE	512
#define OBJ_RING_SIZE	512


/*
 * Return the cache device characteristics.
 */
int
device_characteristics(void *handle, device_char_t *dev_chars)
{
	sdevice_state_t *dev = (sdevice_state_t *)handle;

	*dev_chars = dev->dev_char; 

	/* XXX debug */
	assert(dev_chars->dc_isa == dev->dev_char.dc_isa);
	assert(dev_chars->dc_speed == dev->dev_char.dc_speed);
	assert(dev_chars->dc_mem == dev->dev_char.dc_mem);

	return (0);
}



int
device_statistics(void *handle, dev_stats_t *dev_stats, int *stat_len)
{
	sdevice_state_t *dev = (sdevice_state_t *)handle;

	if (dev->stat_size == 0) { 
		memset(dev_stats, 0, *stat_len);
	} else {
		/* XXX locking ?? */
		if (dev->stat_size > *stat_len) {
			*stat_len = dev->stat_size;
			return(ENOMEM);
		}
		memcpy(dev_stats, dev->dstats, dev->stat_size);
		*stat_len = dev->stat_size;
	}
	return(0);
}



/*
 * This is the entry point to stop a current search.  This build the control
 * header and places it on a queue to be transmitted to the devce.
 */ 

int
device_stop(void *handle, int id)
{
	int			err;
	control_header_t *	cheader;
	sdevice_state_t *dev;

	dev = (sdevice_state_t *)handle;


	cheader = (control_header_t *) malloc(sizeof(*cheader));	
	if (cheader == NULL) {
		/* XXX log */
		return (EAGAIN);
	}

	cheader->generation_number = htonl(id);
	cheader->command = htonl(CNTL_CMD_STOP);
	cheader->data_len = htonl(0);
	cheader->spare = 0;

	err = ring_enq(dev->device_ops, (void *)cheader);
	if (err) {
		/* XXX log */
		/* XXX should we wait ?? */
		printf("XXX failed device stop \n");
		free(cheader);
		return (EAGAIN);
	}

	pthread_mutex_lock(&dev->con_data.mutex);
	dev->con_data.flags |= CINFO_PENDING_CONTROL;
	pthread_mutex_unlock(&dev->con_data.mutex);

	return (0);
}

/*
 * Terminate an ongoing search.  This sends out a termninate request.
 * Once the remote side has finished, then it will send a TERM_DONE
 * request to finish.
 */

int
device_terminate(void *handle, int id)
{
	int			err;
	control_header_t *	cheader;
	sdevice_state_t *dev;

	dev = (sdevice_state_t *)handle;



	cheader = (control_header_t *) malloc(sizeof(*cheader));	
	if (cheader == NULL) {
		/* XXX log */
		return (EAGAIN);
	}

	cheader->generation_number = htonl(id);
	cheader->command = htonl(CNTL_CMD_TERMINATE);
	cheader->data_len = htonl(0);
	cheader->spare = 0;

	err = ring_enq(dev->device_ops, (void *)cheader);
	if (err) {
		/* XXX log */
		/* XXX should we wait ?? */
		free(cheader);
		return (EAGAIN);
	}
	/* XXX flags */
	pthread_mutex_lock(&dev->con_data.mutex);
	dev->con_data.flags |= CINFO_PENDING_CONTROL;
	pthread_mutex_unlock(&dev->con_data.mutex);
	return (0);
}


/*
 * This start a search that has been setup.  
 */


int
device_start(void *handle, int id)
{
	int			err;
	control_header_t *	cheader;
	sdevice_state_t *dev;

	dev = (sdevice_state_t *)handle;

	cheader = (control_header_t *) malloc(sizeof(*cheader));	
	if (cheader == NULL) {
		/* XXX log */
		return (EAGAIN);
	}

	cheader->generation_number = htonl(id);
	cheader->command = htonl(CNTL_CMD_START);
	cheader->data_len = htonl(0);
	cheader->spare = 0;

	err = ring_enq(dev->device_ops, (void *)cheader);
	if (err) {
		/* XXX log */
		/* XXX should we wait ?? */
		printf("XXX failed to enq start  on ring\n");
		free(cheader);
		return (EAGAIN);
	}

	pthread_mutex_lock(&dev->con_data.mutex);
	dev->con_data.flags |= CINFO_PENDING_CONTROL;
	pthread_mutex_unlock(&dev->con_data.mutex);
	return (0);
}

int
device_clear_gids(void *handle, int id)
{
	int			err;
	control_header_t *	cheader;
	sdevice_state_t *dev;

	dev = (sdevice_state_t *)handle;

	cheader = (control_header_t *) malloc(sizeof(*cheader));	
	if (cheader == NULL) {
		/* XXX log */
		return (EAGAIN);
	}

	cheader->generation_number = htonl(id);
	cheader->command = htonl(CNTL_CMD_CLEAR_GIDS);
	cheader->data_len = htonl(0);
	cheader->spare = 0;

	err = ring_enq(dev->device_ops, (void *)cheader);
	if (err) {
		/* XXX log */
		/* XXX should we wait ?? */
		printf("XXX failed to enq clear_gids \n");
		free(cheader);
		return (EAGAIN);
	}

	pthread_mutex_lock(&dev->con_data.mutex);
	dev->con_data.flags |= CINFO_PENDING_CONTROL;
	pthread_mutex_unlock(&dev->con_data.mutex);
	return (0);
}


obj_info_t *
device_next_obj(void *handle)
{
	sdevice_state_t *dev;
	obj_info_t *	oinfo;

	dev = (sdevice_state_t *)handle;
	oinfo = ring_deq(dev->obj_ring);

	if (oinfo != NULL) {
		dev->con_data.flags |= CINFO_PENDING_CREDIT;
	}
	return(oinfo);
}


int
device_new_gid(void *handle, int id, groupid_t gid)
{
	int			err;
	control_header_t *	cheader;
    	sgid_subheader_t *  sgid;
	sdevice_state_t *dev;

	dev = (sdevice_state_t *)handle;

	cheader = (control_header_t *) malloc(sizeof(*cheader));	
	if (cheader == NULL) {
		/* XXX log */
		return (EAGAIN);
	}

	sgid = (sgid_subheader_t *) malloc(sizeof(*sgid));	
    	assert(sgid != NULL);
    	sgid->sgid_gid = gid;

	cheader->generation_number = htonl(id);
	cheader->command = htonl(CNTL_CMD_ADD_GID);
	cheader->data_len = htonl(sizeof(*sgid));
	cheader->spare = (uint32_t) sgid;	

	err = ring_enq(dev->device_ops, (void *)cheader);
	if (err) {
		/* XXX log */
		/* XXX should we wait ?? */
		printf("XXX failed to enq set gids \n");
		free(cheader);
		return (EAGAIN);
	}

	pthread_mutex_lock(&dev->con_data.mutex);
	dev->con_data.flags |= CINFO_PENDING_CONTROL;
	pthread_mutex_unlock(&dev->con_data.mutex);
	return (0);
}

int
device_set_offload(void *handle, int id, uint64_t offload)
{
	int			err;
	control_header_t *	cheader;
    	offload_subheader_t *   offl;
	sdevice_state_t *dev;

	dev = (sdevice_state_t *)handle;

	cheader = (control_header_t *) malloc(sizeof(*cheader));	
	if (cheader == NULL) {
		/* XXX log */
		return (EAGAIN);
	}

	offl = (offload_subheader_t *) malloc(sizeof(*offl));	
    	assert(offl != NULL);
    	offl->offl_data = offload;	/* XXX bswap */

	cheader->generation_number = htonl(id);
	cheader->command = htonl(CNTL_CMD_SET_OFFLOAD);
	cheader->data_len = htonl(sizeof(*offl));
	cheader->spare = (uint32_t) offl;

	err = ring_enq(dev->device_ops, (void *)cheader);
	if (err) {
		/* XXX log */
		/* XXX should we wait ?? */
		printf("XXX failed to enq set gids \n");
		free(cheader);
		free(offl);
		return (EAGAIN);
	}

	pthread_mutex_lock(&dev->con_data.mutex);
	dev->con_data.flags |= CINFO_PENDING_CONTROL;
	pthread_mutex_unlock(&dev->con_data.mutex);
	return (0);
} 

/*
 * This builds the command to set the searchlet on the remote device.
 * This builds the buffers and copies the contents of the files into
 * the buffers.
 */
 

int
device_set_searchlet(void *handle, int id, char *filter, char *spec)
{
	int			err;
	control_header_t *	cheader;
	searchlet_subhead_t *	shead;
	char *			data;
	int			total_len;
	int			spec_len;
	int			filter_len;
	struct stat 		stats;
	ssize_t			rsize;
	FILE *			cur_file;
	sdevice_state_t *	dev;

	dev = (sdevice_state_t *)handle;

	cheader = (control_header_t *) malloc(sizeof(*cheader));	
	if (cheader == NULL) {
		/* XXX log */
		return (EAGAIN);
	}

	cheader->generation_number = htonl(id);
	cheader->command = htonl(CNTL_CMD_SET_SEARCHLET);


	/*
	 * Now get the total size of the filter spec and the filter
	 */
	err = stat(filter, &stats);
	if (err) {
		/* XXX log */
		return (ENOENT);
	}
	filter_len = stats.st_size;

	err = stat(spec, &stats);
	if (err) {
		/* XXX log */
		return (ENOENT);
	}
	spec_len = stats.st_size;


	total_len = ((spec_len + 3) & ~3) + filter_len + sizeof(*shead);
	cheader->data_len = htonl(total_len);

	shead = (searchlet_subhead_t *) malloc(total_len);
	if (shead == NULL) {
		free(cheader);
		return (EAGAIN);
	}

	shead->spec_len = htonl(spec_len);
	shead->filter_len = htonl(filter_len);
	

	/* 
	 * set data to the beginning of the data portion  and
	 * copy in the filter spec from the file.  NOTE: This is
	 * currently blocks, we may want to do something else later.
	 */
	data = (char *)shead + sizeof(*shead);
	cur_file = fopen(spec, "r");
	if (cur_file == NULL) {
		/* XXX log */
		free(cheader);
		free(shead);
		return (ENOENT);
	}
	rsize = fread(data, spec_len, 1, cur_file);
	if (rsize != 1) {
		/* XXX log */
		free(cheader);
		free(shead);
		return(EAGAIN);
	}

	fclose(cur_file);


	/*
	 * Now set data to where we are going to store the searchlet.
	 * Data is the size of the spec_len rounded up to the next 4-byte
	 * boundary.
	 */
	data += ((spec_len + 3) & ~3);
	cur_file = fopen(filter, "r");
	if (cur_file == NULL) {
		/* XXX log */
		free(cheader);
		free(shead);
		return (ENOENT);
	}
	rsize = fread(data, filter_len, 1, cur_file);
	if (rsize != 1) {
		/* XXX log */
		free(cheader);
		free(shead);
		return(EAGAIN);
	}

	fclose(cur_file);


	/*
	 * XXX HACK.  For now we store the pointer to the data
	 * using the spare field in the header to point to the data.
	 */
	cheader->spare = (uint32_t) shead;	

	err = ring_enq(dev->device_ops, (void *)cheader);
	if (err) {
		/* XXX log */
		/* XXX should we wait ?? */
		free(cheader);
		return (EAGAIN);
	}
	pthread_mutex_lock(&dev->con_data.mutex);
	dev->con_data.flags |= CINFO_PENDING_CONTROL;
	pthread_mutex_unlock(&dev->con_data.mutex);
	return (0);
}


int
device_set_log(void *handle, uint32_t level, uint32_t src)
{
	int			err;
	control_header_t *	cheader;
	sdevice_state_t *dev;
	setlog_subheader_t *	slheader;

	dev = (sdevice_state_t *)handle;

	cheader = (control_header_t *) malloc(sizeof(*cheader));	
	if (cheader == NULL) {
		/* XXX log */
		return (EAGAIN);
	}

	slheader = (setlog_subheader_t *) malloc(sizeof(*slheader));	
	if (slheader == NULL) {
		/* XXX log */
		free(cheader);
		return (EAGAIN);
	}



	cheader->generation_number = htonl(0);	/* XXX */
	cheader->command = htonl(CNTL_CMD_SETLOG);
	cheader->data_len = htonl(sizeof(*slheader));
	cheader->spare = (uint32_t) slheader;	

	slheader->log_level = level;
	slheader->log_src = src;



	err = ring_enq(dev->device_ops, (void *)cheader);
	if (err) {
		/* XXX log */
		/* XXX should we wait ?? */
		printf("XXX failed to enq set_log \n");
		free(cheader);
		return (EAGAIN);
	}

	pthread_mutex_lock(&dev->con_data.mutex);
	dev->con_data.flags |= CINFO_PENDING_CONTROL;
	pthread_mutex_unlock(&dev->con_data.mutex);
	return (0);
}




int 
device_write_leaf(void *handle, char *path, int len, char *data, int32_t opid)
{
	int		        	    err;
	control_header_t *  	cheader;
	sdevice_state_t *       dev;
	dctl_subheader_t *      dsub;
    int                     plen;
    int                     tot_len;

	dev = (sdevice_state_t *)handle;

    plen = strlen(path) + 1;
    tot_len = plen + len + sizeof(*dsub);

	cheader = (control_header_t *) malloc(sizeof(*cheader));	
	if (cheader == NULL) {
		/* XXX log */
		return (EAGAIN);
	}

    dsub = (dctl_subheader_t *) malloc(tot_len);
	if (dsub == NULL) {
		/* XXX log */
        free(cheader);
		return (EAGAIN);
	}


    /* fill in the data */

	cheader->generation_number = htonl(0);
	cheader->command = htonl(CNTL_CMD_WRITE_LEAF);
	cheader->data_len = htonl(tot_len);
	/*
	 * XXX HACK.  For now we store the pointer to the data
	 * using the spare field in the header to point to the data.
	 */
	cheader->spare = (uint32_t) dsub;	

    /*
     * Fill in the subheader.
     */
    dsub->dctl_err = htonl(0);
    dsub->dctl_opid = htonl(opid);
    dsub->dctl_plen = htonl(plen);
    dsub->dctl_dlen = htonl(len);
    memcpy(&dsub->dctl_data[0], path, plen);
    memcpy(&dsub->dctl_data[plen], data, len);


	err = ring_enq(dev->device_ops, (void *)cheader);
	if (err) {
		/* XXX log */
		/* XXX should we wait ?? */
		printf("XXX failed to write leaf \n");
		free(cheader);
        free(dsub);
		return (EAGAIN);
	}

	pthread_mutex_lock(&dev->con_data.mutex);
	dev->con_data.flags |= CINFO_PENDING_CONTROL;
	pthread_mutex_unlock(&dev->con_data.mutex);
	return (0);
}

int 
device_read_leaf(void *handle, char *path, int32_t opid)
{
	int		        	    err;
	control_header_t *  	cheader;
	sdevice_state_t *       dev;
	dctl_subheader_t *      dsub;
    int                     plen;
    int                     tot_len;

	dev = (sdevice_state_t *)handle;

    plen = strlen(path) + 1;
    tot_len = plen + sizeof(*dsub);

	cheader = (control_header_t *) malloc(sizeof(*cheader));	
	if (cheader == NULL) {
		/* XXX log */
		return (EAGAIN);
	}

    dsub = (dctl_subheader_t *) malloc(tot_len);
	if (dsub == NULL) {
		/* XXX log */
        free(cheader);
		return (EAGAIN);
	}


    /* fill in the data */

	cheader->generation_number = htonl(0);
	cheader->command = htonl(CNTL_CMD_READ_LEAF);
	cheader->data_len = htonl(tot_len);
	/*
	 * XXX HACK.  For now we store the pointer to the data
	 * using the spare field in the header to point to the data.
	 */
	cheader->spare = (uint32_t) dsub;	

    /*
     * Fill in the subheader.
     */
    dsub->dctl_err = htonl(0);
    dsub->dctl_opid = htonl(opid);
    dsub->dctl_plen = htonl(plen);
    dsub->dctl_dlen = htonl(0);
    memcpy(&dsub->dctl_data[0], path, plen);


	err = ring_enq(dev->device_ops, (void *)cheader);
	if (err) {
		/* XXX log */
		/* XXX should we wait ?? */
		printf("XXX failed to write leaf \n");
		free(cheader);
        free(dsub);
		return (EAGAIN);
	}

	pthread_mutex_lock(&dev->con_data.mutex);
	dev->con_data.flags |= CINFO_PENDING_CONTROL;
	pthread_mutex_unlock(&dev->con_data.mutex);
	return (0);

}


int 
device_list_nodes(void *handle, char *path, int32_t opid)
{
	int		        	    err;
	control_header_t *  	cheader;
	sdevice_state_t *       dev;
	dctl_subheader_t *      dsub;
    int                     plen;
    int                     tot_len;

	dev = (sdevice_state_t *)handle;

    plen = strlen(path) + 1;
    tot_len = plen + sizeof(*dsub);

	cheader = (control_header_t *) malloc(sizeof(*cheader));	
	if (cheader == NULL) {
		/* XXX log */
		return (EAGAIN);
	}

    dsub = (dctl_subheader_t *) malloc(tot_len);
	if (dsub == NULL) {
		/* XXX log */
        free(cheader);
		return (EAGAIN);
	}


    /* fill in the data */

	cheader->generation_number = htonl(0);
	cheader->command = htonl(CNTL_CMD_LIST_NODES);
	cheader->data_len = htonl(tot_len);
	/*
	 * XXX HACK.  For now we store the pointer to the data
	 * using the spare field in the header to point to the data.
	 */
	cheader->spare = (uint32_t) dsub;	

    /*
     * Fill in the subheader.
     */
    dsub->dctl_err = htonl(0);
    dsub->dctl_opid = htonl(opid);
    dsub->dctl_plen = htonl(plen);
    dsub->dctl_dlen = htonl(0);
    memcpy(&dsub->dctl_data[0], path, plen);


	err = ring_enq(dev->device_ops, (void *)cheader);
	if (err) {
		/* XXX log */
		/* XXX should we wait ?? */
		printf("XXX failed to write leaf \n");
		free(cheader);
        free(dsub);
		return (EAGAIN);
	}

	pthread_mutex_lock(&dev->con_data.mutex);
	dev->con_data.flags |= CINFO_PENDING_CONTROL;
	pthread_mutex_unlock(&dev->con_data.mutex);
	return (0);
}

int 
device_list_leafs(void *handle, char *path, int32_t opid)
{
	int		        	    err;
	control_header_t *  	cheader;
	sdevice_state_t *       dev;
	dctl_subheader_t *      dsub;
    	int                     plen;
    	int                     tot_len;

	dev = (sdevice_state_t *)handle;

    	plen = strlen(path) + 1;
    	tot_len = plen + sizeof(*dsub);

	cheader = (control_header_t *) malloc(sizeof(*cheader));	
	if (cheader == NULL) {
		/* XXX log */
		return (EAGAIN);
	}

    	dsub = (dctl_subheader_t *) malloc(tot_len);
	if (dsub == NULL) {
		/* XXX log */
        free(cheader);
		return (EAGAIN);
	}


    /* fill in the data */

	cheader->generation_number = htonl(0);
	cheader->command = htonl(CNTL_CMD_LIST_LEAFS);
	cheader->data_len = htonl(tot_len);
	/*
	 * XXX HACK.  For now we store the pointer to the data
	 * using the spare field in the header to point to the data.
	 */
	cheader->spare = (uint32_t) dsub;	

	/*
	 * Fill in the subheader.
	 */
	dsub->dctl_err = htonl(0);
	dsub->dctl_opid = htonl(opid);
	dsub->dctl_plen = htonl(plen);
	dsub->dctl_dlen = htonl(0);
	memcpy(&dsub->dctl_data[0], path, plen);


	err = ring_enq(dev->device_ops, (void *)cheader);
	if (err) {
		/* XXX log */
		/* XXX should we wait ?? */
		printf("XXX failed to write leaf \n");
		free(cheader);
        free(dsub);
		return (EAGAIN);
	}

	pthread_mutex_lock(&dev->con_data.mutex);
	dev->con_data.flags |= CINFO_PENDING_CONTROL;
	pthread_mutex_unlock(&dev->con_data.mutex);
	return (0);
}

int 
device_set_blob(void *handle, int id, char *name, int blob_len, void *blob)
{
	int		        	    err;
	control_header_t *  	cheader;
	sdevice_state_t *       dev;
	blob_subheader_t *      bsub;
    	int                     nlen;
    	int                     tot_len;

	dev = (sdevice_state_t *)handle;

    	nlen = strlen(name) + 1;
    	tot_len = nlen + blob_len + sizeof(*bsub);

	cheader = (control_header_t *) malloc(sizeof(*cheader));	
	if (cheader == NULL) {
		/* XXX log */
		return (EAGAIN);
	}

    	bsub = (blob_subheader_t *) malloc(tot_len);
	if (bsub == NULL) {
		/* XXX log */
        free(cheader);
		return (EAGAIN);
	}


	/* fill in the data */

	cheader->generation_number = htonl(id);	
	cheader->command = htonl(CNTL_CMD_SET_BLOB);
	cheader->data_len = htonl(tot_len);
	/*
	 * XXX HACK.  For now we store the pointer to the data
	 * using the spare field in the header to point to the data.
	 */
	cheader->spare = (uint32_t) bsub;	

	/*
	 * Fill in the subheader.
	 */
	bsub->blob_nlen = htonl(nlen);
	bsub->blob_blen = htonl(blob_len);
	memcpy(&bsub->blob_sdata[0], name, nlen);
	memcpy(&bsub->blob_sdata[nlen], blob, blob_len);


	err = ring_enq(dev->device_ops, (void *)cheader);
	if (err) {
		/* XXX log */
		/* XXX should we wait ?? */
		printf("XXX failed to write leaf \n");
		free(cheader);
        free(bsub);
		return (EAGAIN);
	}

	pthread_mutex_lock(&dev->con_data.mutex);
	dev->con_data.flags |= CINFO_PENDING_CONTROL;
	pthread_mutex_unlock(&dev->con_data.mutex);
	return (0);
}

int 
device_set_limit(void *handle, int limit)
{
	sdevice_state_t *       dev;
	dev = (sdevice_state_t *)handle;

	if (dev->con_data.obj_limit != limit) {
		dev->con_data.flags |= CINFO_PENDING_CREDIT;
		dev->con_data.obj_limit = limit;
	}

	return (0);
}



int 
device_stop_obj(void *handle)
{
	sdevice_state_t *       dev = (sdevice_state_t *)handle;

	pthread_mutex_lock(&dev->con_data.mutex);
	dev->con_data.flags |= CINFO_BLOCK_OBJ;
	pthread_mutex_unlock(&dev->con_data.mutex);
}

int 
device_enable_obj(void *handle)
{
	sdevice_state_t *       dev = (sdevice_state_t *)handle;

	pthread_mutex_lock(&dev->con_data.mutex);
	dev->con_data.flags &= ~CINFO_BLOCK_OBJ;
	pthread_mutex_unlock(&dev->con_data.mutex);
}

static void
setup_stats(sdevice_state_t *dev, uint32_t devid)
{
    struct hostent *hent;
    int             len, err;
    char *          delim;
    char            node_name[128]; /* XXX */
    char            path_name[128]; /* XXX */

    hent = gethostbyaddr(&devid, sizeof(devid), AF_INET);
    if (hent == NULL) {
        struct in_addr in;

        printf("failed to get hostname\n");
        in.s_addr = devid;
        delim = inet_ntoa(in);
        strcpy(node_name, delim);

        /* replace all the '.' with '_' */
        while ((delim = index(node_name, '.')) != NULL) {
            *delim = '_';
        }
    } else {
        delim = index(hent->h_name ,'.');
        if (delim == NULL) {
            len = strlen(hent->h_name);
        } else {
            len = delim - hent->h_name;
        }
        strncpy(node_name, hent->h_name , len);
        node_name[len] = 0;
    }

	sprintf(path_name, "%s.%s", HOST_NETWORK_PATH, node_name);

	err = dctl_register_node(HOST_NETWORK_PATH, node_name);
	assert(err==0);


       
    dctl_register_leaf(path_name, "obj_rx", DCTL_DT_UINT32, 
            dctl_read_uint32, NULL, &dev->con_data.stat_obj_rx);
    dctl_register_leaf(path_name, "obj_total_bytes_rx", DCTL_DT_UINT64, 
            dctl_read_uint64, NULL, &dev->con_data.stat_obj_total_byte_rx);
    dctl_register_leaf(path_name, "obj_hdr_bytes_rx", DCTL_DT_UINT64, 
            dctl_read_uint64, NULL, &dev->con_data.stat_obj_hdr_byte_rx);
    dctl_register_leaf(path_name, "obj_attr_bytes_rx", DCTL_DT_UINT64, 
            dctl_read_uint64, NULL, &dev->con_data.stat_obj_attr_byte_rx);
    dctl_register_leaf(path_name, "obj_data_bytes_rx", DCTL_DT_UINT64, 
            dctl_read_uint64, NULL, &dev->con_data.stat_obj_data_byte_rx);

    dctl_register_leaf(path_name, "control_rx", DCTL_DT_UINT32, 
            dctl_read_uint32, NULL, &dev->con_data.stat_control_rx);
    dctl_register_leaf(path_name, "control_byte_rx", DCTL_DT_UINT64, 
            dctl_read_uint64, NULL, &dev->con_data.stat_control_byte_rx);
    dctl_register_leaf(path_name, "control_tx", DCTL_DT_UINT32, 
            dctl_read_uint32, NULL, &dev->con_data.stat_control_tx);
    dctl_register_leaf(path_name, "control_byte_tx", DCTL_DT_UINT64, 
            dctl_read_uint64, NULL, &dev->con_data.stat_control_byte_tx);
    dctl_register_leaf(path_name, "log_rx", DCTL_DT_UINT32, 
            dctl_read_uint32, NULL, &dev->con_data.stat_log_rx);
    dctl_register_leaf(path_name, "log_byte_rx", DCTL_DT_UINT64, 
            dctl_read_uint64, NULL, &dev->con_data.stat_log_byte_rx);

}

/*
 * This is the initialization function that is
 * called by the searchlet library when we startup.
 */

/* XXX callback for new packets  */
void *
device_init(int id, uint32_t devid, void *hcookie, hstub_cb_args_t *cb_list,
	void *dctl_cookie, void *log_cookie)
{
	sdevice_state_t *new_dev;
	int		err;

	new_dev = (sdevice_state_t *) malloc(sizeof(*new_dev));	
	if (new_dev == NULL) {
		return (NULL);
	}

	memset(new_dev, 0, sizeof(*new_dev));

	new_dev->log_cookie = log_cookie;
	new_dev->dctl_cookie = dctl_cookie;
	/*
	 * initialize the ring that is used to queue "commands"
	 * to the background thread.
	 */
	err = ring_init(&new_dev->device_ops, HSTUB_RING_SIZE);
	if (err) {
		free(new_dev);
		return (NULL);
	}

	err = ring_init(&new_dev->obj_ring, OBJ_RING_SIZE);
	if (err) {
		free(new_dev);
		return (NULL);
	}


	new_dev->flags = 0;

	pthread_mutex_init(&new_dev->con_data.mutex, NULL);

	/*
	 * Open the sockets to the new host.
	 */
	err = hstub_establish_connection(&new_dev->con_data, devid);
	if (err) {
		/* XXX log,  */
		free(new_dev);
		return(NULL);
	}

	/*
	 * Save the callback and the host cookie.
	 */
	new_dev->hcookie = hcookie;
	new_dev->hstub_log_data_cb = cb_list->log_data_cb;
	new_dev->hstub_search_done_cb = cb_list->search_done_cb;
	new_dev->hstub_rleaf_done_cb = cb_list->rleaf_done_cb;
	new_dev->hstub_wleaf_done_cb = cb_list->wleaf_done_cb;
	new_dev->hstub_lnode_done_cb = cb_list->lnode_done_cb;
	new_dev->hstub_lleaf_done_cb = cb_list->lleaf_done_cb;


	/*
	 * Init caches stats.
	 */
	new_dev->dstats = NULL;
	new_dev->stat_size = 0;

	

	setup_stats(new_dev, devid); 

	/*
	 * Spawn a thread for this device that process data to and
	 * from the device.
	 */

	err = pthread_create(&new_dev->thread_id, PATTR_DEFAULT, hstub_main, 
			    (void *)new_dev);
	if (err) {
		/* XXX log */
		free(new_dev);
		return (NULL);
	}
	return((void *)new_dev);
}




/*
 * This is used to tear down the state assocaited with the
 * device search.
 */
void
device_fini(sdevice_state_t *dev_state)
{

	free(dev_state); 
}

