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
#include <string.h>
#include <sys/time.h>
#include <netdb.h>
#include <assert.h>
#include "diamond_consts.h"
#include "diamond_types.h"
#include "lib_tools.h"
#include "lib_log.h"
#include "socket_trans.h"
#include "obj_attr.h"
#include "lib_odisk.h"
#include "lib_dctl.h"
#include "dctl_common.h"
#include "lib_hstub.h"
#include "lib_auth.h"
#include "lib_dconfig.h"
#include "hstub_impl.h"
#include "rpc_client_content.h"
#include "rpc_preamble.h"

static char const cvsid[] =
    "$Header$";

/*
 * XXX move to common header 
 */
#define	HSTUB_RING_SIZE	512
#define OBJ_RING_SIZE	512


/*
 * Return the cache device characteristics.
 */
int
device_characteristics(void *handle, device_char_t * dev_chars)
{
	sdevice_state_t *dev = (sdevice_state_t *) handle;

	*dev_chars = dev->dev_char;

	/*
	 * XXX debug 
	 */
	assert(dev_chars->dc_isa == dev->dev_char.dc_isa);
	assert(dev_chars->dc_speed == dev->dev_char.dc_speed);
	assert(dev_chars->dc_mem == dev->dev_char.dc_mem);

	return (0);
}



int
device_statistics(void *handle, dev_stats_t * dev_stats, int *stat_len)
{
	sdevice_state_t *dev = (sdevice_state_t *) handle;

	if (dev->stat_size == 0) {
		memset(dev_stats, 0, *stat_len);
	} else {
		/*
		 * XXX locking ?? 
		 */
		if (dev->stat_size > *stat_len) {
			*stat_len = dev->stat_size;
			return (ENOMEM);
		}
		memcpy(dev_stats, dev->dstats, dev->stat_size);
		*stat_len = dev->stat_size;
	}
	return (0);
}



/*
 * This is the entry point to stop a current search.  This build the control
 * header and places it on a queue to be transmitted to the devce.
 */

int
device_stop(void *handle, int id, host_stats_t *hs)
{
	diamond_rc_t *rc;
	stop_x sx;
	sdevice_state_t *dev;


	dev = (sdevice_state_t *) handle;

	rc = device_stop_x_2(id, sx, dev->con_data.tirpc_client);
	if (rc == (diamond_rc_t *) NULL) {
	  log_message(LOGT_NET, LOGL_ERR, "request_chars: call sending failed");
	  return -1;
	}
	if(rc->service_err != DIAMOND_SUCCESS) {
	  log_message(LOGT_NET, LOGL_ERR, "request_chars: call servicing failed");
	  log_message(LOGT_NET, LOGL_ERR, diamond_error(rc));
	  return -1;
	}

	sx.host_objs_received = hs->hs_objs_received;
	sx.host_objs_queued = hs->hs_objs_queued;
	sx.host_objs_read = hs->hs_objs_read;
	sx.app_objs_queued = hs->hs_objs_uqueued;
	sx.app_objs_presented = hs->hs_objs_upresented;

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
	diamond_rc_t *rc;
	sdevice_state_t *dev;

	dev = (sdevice_state_t *) handle;

	rc = device_clear_gids_x_2(id, dev->con_data.tirpc_client);
	if (rc == (diamond_rc_t *) NULL) {
	  log_message(LOGT_NET, LOGL_ERR, "device_clear_gids: call sending failed");
	  return -1;
	}
	if(rc->service_err != DIAMOND_SUCCESS) {
	  log_message(LOGT_NET, LOGL_ERR, "device_clear_gids: call servicing failed");
	  log_message(LOGT_NET, LOGL_ERR, diamond_error(rc));
	  return -1;
	}

	return (0);
}


/*
 * This start a search that has been setup.  
 */


int
device_start(void *handle, int id)
{
	diamond_rc_t *rc;
	sdevice_state_t *dev;

	dev = (sdevice_state_t *) handle;

	/* save the new start id */
	dev->ver_no = id;

	rc = device_start_x_2(id, dev->con_data.tirpc_client);
	if (rc == (diamond_rc_t *) NULL) {
	  log_message(LOGT_NET, LOGL_ERR, "device_start: call sending failed");
	  return -1;
	}
	if(rc->service_err != DIAMOND_SUCCESS) {
	  log_message(LOGT_NET, LOGL_ERR, "device_start: call servicing failed");
	  log_message(LOGT_NET, LOGL_ERR, diamond_error(rc));
	  return -1;
	}

	return (0);
}

int
device_clear_gids(void *handle, int id)
{
	diamond_rc_t *rc;
	sdevice_state_t *dev;

	dev = (sdevice_state_t *) handle;

	rc = device_clear_gids_x_2(id, dev->con_data.tirpc_client);
	if (rc == (diamond_rc_t *) NULL) {
	  log_message(LOGT_NET, LOGL_ERR, "device_clear_gids: call sending failed");
	  return -1;
	}
	if(rc->service_err != DIAMOND_SUCCESS) {
	  log_message(LOGT_NET, LOGL_ERR, "device_clear_gids: call servicing failed");
	  log_message(LOGT_NET, LOGL_ERR, diamond_error(rc));
	  return -1;
	}

	return (0);
}


obj_info_t     *
device_next_obj(void *handle)
{
	sdevice_state_t *dev;
	obj_info_t     *oinfo;

	dev = (sdevice_state_t *) handle;
	oinfo = ring_deq(dev->obj_ring);

	if (oinfo != NULL) {
		dev->con_data.flags |= CINFO_PENDING_CREDIT;
	} else {
	       	if (dev->con_data.cc_counter++ > 100) {
			dev->con_data.flags |= CINFO_PENDING_CREDIT;
			dev->con_data.cc_counter = 0;
		}
	}
	return (oinfo);
}


int
device_new_gid(void *handle, int id, groupid_t gid)
{
	diamond_rc_t *rc;
	sdevice_state_t *dev;
	groupid_x gix;

	dev = (sdevice_state_t *) handle;

	gix = gid;

	rc = device_new_gid_x_2(id, gix, dev->con_data.tirpc_client);
	if (rc == (diamond_rc_t *) NULL) {
	  log_message(LOGT_NET, LOGL_ERR, "device_new_gid: call sending failed");
	  return -1;
	}
	if(rc->service_err != DIAMOND_SUCCESS) {
	  log_message(LOGT_NET, LOGL_ERR, "device_new_gid: call servicing failed");
	  log_message(LOGT_NET, LOGL_ERR, diamond_error(rc));
	  return -1;
	}

	return (0);
}


/*
 * This builds the command to set the searchlet on the remote device.
 * This builds the buffers and copies the contents of the files into
 * the buffers.
 */


int
device_set_spec(void *handle, int id, char *spec, sig_val_t *sig)
{
	int             err;
	char           *data;
	int             spec_len;
	struct stat     stats;
	ssize_t         rsize;
	FILE           *cur_file;
	sdevice_state_t *dev;
	diamond_rc_t   *rc;
	spec_file_x     sf;

	dev = (sdevice_state_t *) handle;

	err = stat(spec, &stats);
	if (err) {
		log_message(LOGT_NET, LOGL_ERR,
		    "device_set_searchlet: failed stat spec file <%s>", 
		    spec);
		return (ENOENT);
	}
	spec_len = stats.st_size;

	if ((data = malloc(spec_len)) == NULL) {
	  log_message(LOGT_NET, LOGL_ERR,
		      "device_set_searchlet: failed open spec <%s>", spec);
	  return ENOENT;
	}

	/*
	 * set data to the beginning of the data portion  and
	 * copy in the filter spec from the file.  NOTE: This is
	 * currently blocks, we may want to do something else later.
	 */
	if ((cur_file = fopen(spec, "r")) == NULL) {
		log_message(LOGT_NET, LOGL_ERR,
		    "device_set_searchlet: failed open spec <%s>", spec);
		free(data);
		return (ENOENT);
	}
	if ((rsize = fread(data, spec_len, 1, cur_file)) != 1) {
		log_message(LOGT_NET, LOGL_ERR,
		    "device_set_searchlet: failed read spec <%s>", 
		    spec);
		free(data);
		return (EAGAIN);
	}

	fclose(cur_file);

	sf.sig.sig_val_x_len = sizeof(sig_val_t);
	sf.sig.sig_val_x_val = (char *)sig;

	sf.data.data_len = spec_len;
	sf.data.data_val = data;

	rc = device_set_spec_x_2(id, sf, dev->con_data.tirpc_client);
	if (rc == (diamond_rc_t *) NULL) {
	  log_message(LOGT_NET, LOGL_ERR, "device_new_gid: call sending failed");
	  free(data);
	  return ENOENT;
	}
	if(rc->service_err != DIAMOND_SUCCESS) {
	  log_message(LOGT_NET, LOGL_ERR, "device_new_gid: call servicing failed");
	  log_message(LOGT_NET, LOGL_ERR, diamond_error(rc));
	  free(data);
	  return ENOENT;
	}
	
	free(data);

	return (0);
}

int
device_set_lib(void *handle, int id, sig_val_t *obj_sig)
{
	diamond_rc_t *rc;
	sdevice_state_t *dev;
	sig_val_x sx;
	send_obj_x ox;
	struct stat     stats;
	ssize_t         rsize;
	int		buf_len, err;
	FILE           *cur_file;
	char objname[PATH_MAX];
	char *cache;
	char *data;
	char *sig;


	dev = (sdevice_state_t *) handle;

	memcpy(&sx, obj_sig, sizeof(*obj_sig));

	rc = device_set_obj_x_2(id, sx, dev->con_data.tirpc_client);
	if (rc == (diamond_rc_t *) NULL) {
	  log_message(LOGT_NET, LOGL_ERR, "device_new_gid: call sending failed");
	  return -1;
	}
	if(rc->service_err != DIAMOND_SUCCESS) {
	  if(!((rc->service_err == DIAMOND_OPERR) && 
	       (rc->opcode_err == DIAMOND_OPCODE_FCACHEMISS))) {
	    log_message(LOGT_NET, LOGL_ERR, "device_new_gid: call servicing failed");
	    log_message(LOGT_NET, LOGL_ERR, diamond_error(rc));
	    return -1;
	  }
	}
	else return 0;

	/* If we've reached this point, the server does not have this
	 * filter library and we need to make another send_obj call. */

	cache = dconf_get_binary_cachedir();
	sig = sig_string(obj_sig);
	snprintf(objname, PATH_MAX, OBJ_FORMAT, cache, sig);

	assert(file_exists(objname));

	err = stat(objname, &stats);
	if (err) {
		log_message(LOGT_NET, LOGL_ERR,
		    "device_set_lib: failed stat spec file <%s>",  
		    objname);
		return (ENOENT);
	}
	buf_len = stats.st_size;


	ox.obj_sig.sig_val_x_val = (char *)obj_sig;
	ox.obj_sig.sig_val_x_len = sizeof(sig_val_t);

	data = (char *)malloc(buf_len);
	if(data == NULL) {
	  perror("malloc");
	  log_message(LOGT_NET, LOGL_ERR,
		      "device_set_lib: failed malloc spec file <%s>",  
		      objname);
	  return ENOENT;
	}


	/*
	 * set data to the beginning of the data portion  and
	 * copy in the filter spec from the file.  NOTE: This is
	 * currently blocks, we may want to do something else later.
	 */

	if ((cur_file = fopen(objname, "r")) == NULL) {
		log_message(LOGT_NET, LOGL_ERR,
		    "device_set_lib: failed open <%s>", objname);
		return (ENOENT);
	}
	if ((rsize = fread(data, buf_len, 1, cur_file)) != 1) {
		log_message(LOGT_NET, LOGL_ERR,
		    "device_set_lib: failed read obj <%s>", objname);
		return (EAGAIN);
	}

	fclose(cur_file);

	ox.obj_data.obj_data_len = buf_len;
	ox.obj_data.obj_data_val = data;

	rc = device_send_obj_x_2(id, ox, dev->con_data.tirpc_client);
	if (rc == (diamond_rc_t *) NULL) {
	  log_message(LOGT_NET, LOGL_ERR, "device_set_lib: send_obj call sending failed");
	  free(data);
	  return -1;
	}
	if(rc->service_err != DIAMOND_SUCCESS) {
	  log_message(LOGT_NET, LOGL_ERR, "device_set_lib: send_obj call servicing failed");
	  log_message(LOGT_NET, LOGL_ERR, diamond_error(rc));
	  free(data);
	  return -1;
	}

	return (0);
}



int
device_write_leaf(void *handle, char *path, int len, char *data, int32_t opid)
{
	sdevice_state_t *dev;
	dctl_x          dx;
	int             plen;
	diamond_rc_t    *rc;

	dev = (sdevice_state_t *) handle;

	plen = strlen(path) + 1;

	if((data = malloc(plen+len)) == NULL) {
	  log_message(LOGT_NET, LOGL_ERR,
		      "device_write_leaf: failed malloc data");
	  return (EAGAIN);
	}

	memcpy(&data[0], path, plen);
	memcpy(&data[plen], data, len);

	dx.dctl_err = 0;
	dx.dctl_opid = opid;
	dx.dctl_plen = plen;
	dx.dctl_data.dctl_data_len = plen+len;
	dx.dctl_data.dctl_data_val = data;

	rc = device_write_leaf_x_2(0, dx, dev->con_data.tirpc_client);
	if (rc == (diamond_rc_t *) NULL) {
	  log_message(LOGT_NET, LOGL_ERR, "device_new_gid: call sending failed");
	  return -1;
	}
	if(rc->service_err != DIAMOND_SUCCESS) {
	  log_message(LOGT_NET, LOGL_ERR, "device_new_gid: call servicing failed");
	  log_message(LOGT_NET, LOGL_ERR, diamond_error(rc));
	  return -1;
	}

	free(data);

	return (0);
}

int
device_read_leaf(void *handle, char *path, int32_t opid)
{
	int             err;
	control_header_t *cheader;
	sdevice_state_t *dev;
	dctl_subheader_t *dsub;
	int             plen;
	int             tot_len;

	dev = (sdevice_state_t *) handle;

	plen = strlen(path) + 1;
	tot_len = plen + sizeof(*dsub);

	cheader = (control_header_t *) malloc(sizeof(*cheader));
	if (cheader == NULL) {
		log_message(LOGT_NET, LOGL_ERR,
		    "device_read_leaf: failed malloc command");
		return (EAGAIN);
	}

	dsub = (dctl_subheader_t *) malloc(tot_len);
	if (dsub == NULL) {
		log_message(LOGT_NET, LOGL_ERR,
		    "device_read_leaf: failed malloc data");
		free(cheader);
		return (EAGAIN);
	}


	/*
	 * fill in the data 
	 */

	cheader->generation_number = htonl(0);
	cheader->command = htonl(CNTL_CMD_READ_LEAF);
	cheader->data_len = htonl(tot_len);
	cheader->spare = (uint32_t) dsub;

	/*
	 * Fill in the subheader.
	 */
	dsub->dctl_err = htonl(0);
	dsub->dctl_opid = htonl(opid);
	dsub->dctl_plen = htonl(plen);
	dsub->dctl_dlen = htonl(0);
	memcpy(&dsub->dctl_data[0], path, plen);


	err = ring_enq(dev->device_ops, (void *) cheader);
	if (err) {
		log_message(LOGT_NET, LOGL_ERR,
		    "device_read_leaf: failed enqueue command");
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
	int             err;
	control_header_t *cheader;
	sdevice_state_t *dev;
	dctl_subheader_t *dsub;
	int             plen;
	int             tot_len;

	dev = (sdevice_state_t *) handle;

	plen = strlen(path) + 1;
	tot_len = plen + sizeof(*dsub);

	cheader = (control_header_t *) malloc(sizeof(*cheader));
	if (cheader == NULL) {
		log_message(LOGT_NET, LOGL_ERR,
		    "device_list_nodes: failed malloc command");
		return (EAGAIN);
	}

	dsub = (dctl_subheader_t *) malloc(tot_len);
	if (dsub == NULL) {
		log_message(LOGT_NET, LOGL_ERR,
		    "device_list_nodes: failed malloc data");
		free(cheader);
		return (EAGAIN);
	}


	/*
	 * fill in the data 
	 */

	cheader->generation_number = htonl(0);
	cheader->command = htonl(CNTL_CMD_LIST_NODES);
	cheader->data_len = htonl(tot_len);
	cheader->spare = (uint32_t) dsub;

	/*
	 * Fill in the subheader.
	 */
	dsub->dctl_err = htonl(0);
	dsub->dctl_opid = htonl(opid);
	dsub->dctl_plen = htonl(plen);
	dsub->dctl_dlen = htonl(0);
	memcpy(&dsub->dctl_data[0], path, plen);


	err = ring_enq(dev->device_ops, (void *) cheader);
	if (err) {
		log_message(LOGT_NET, LOGL_ERR,
		    "device_list_nodes: failed enqueue command");
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
	int             err;
	control_header_t *cheader;
	sdevice_state_t *dev;
	dctl_subheader_t *dsub;
	int             plen;
	int             tot_len;

	dev = (sdevice_state_t *) handle;

	plen = strlen(path) + 1;
	tot_len = plen + sizeof(*dsub);

	cheader = (control_header_t *) malloc(sizeof(*cheader));
	if (cheader == NULL) {
		log_message(LOGT_NET, LOGL_ERR,
		    "device_list_leafs: failed malloc header");
		return (EAGAIN);
	}

	dsub = (dctl_subheader_t *) malloc(tot_len);
	if (dsub == NULL) {
		log_message(LOGT_NET, LOGL_ERR,
		    "device_list_leafs: failed malloc data");
		free(cheader);
		return (EAGAIN);
	}


	/*
	 * fill in the data 
	 */

	cheader->generation_number = htonl(0);
	cheader->command = htonl(CNTL_CMD_LIST_LEAFS);
	cheader->data_len = htonl(tot_len);
	cheader->spare = (uint32_t) dsub;

	/*
	 * Fill in the subheader.
	 */
	dsub->dctl_err = htonl(0);
	dsub->dctl_opid = htonl(opid);
	dsub->dctl_plen = htonl(plen);
	dsub->dctl_dlen = htonl(0);
	memcpy(&dsub->dctl_data[0], path, plen);


	err = ring_enq(dev->device_ops, (void *) cheader);
	if (err) {
		log_message(LOGT_NET, LOGL_ERR,
		    "device_list_leafs: failed enqueue command");
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
	sdevice_state_t *dev;
	blob_x           bx;
	diamond_rc_t    *rc;

	dev = (sdevice_state_t *) handle;

	bx.blob_name.blob_name_len = strlen(name) + 1;
	bx.blob_name.blob_name_val = name;
	bx.blob_data.blob_data_len = blob_len;
	bx.blob_data.blob_data_val = blob;

	rc = device_set_blob_x_2(id, bx, dev->con_data.tirpc_client);
	if (rc == (diamond_rc_t *) NULL) {
	  log_message(LOGT_NET, LOGL_ERR, "device_set_blob: call sending failed");
	  return -1;
	}
	if(rc->service_err != DIAMOND_SUCCESS) {
	  log_message(LOGT_NET, LOGL_ERR, "device_set_blob: call servicing failed");
	  log_message(LOGT_NET, LOGL_ERR, diamond_error(rc));
	  return -1;
	}

	return (0);
}

int
device_set_limit(void *handle, int limit)
{
	sdevice_state_t *dev;
	dev = (sdevice_state_t *) handle;

	if (dev->con_data.obj_limit != limit) {
		dev->con_data.flags |= CINFO_PENDING_CREDIT;
		dev->con_data.obj_limit = limit;
	}

	return (0);
}

int
device_set_exec_mode(void *handle, int id, uint32_t mode) 
{
	sdevice_state_t *dev;
	diamond_rc_t *rc;

	dev = (sdevice_state_t *) handle;

	rc = device_set_exec_mode_x_2(id, mode, dev->con_data.tirpc_client);
	if (rc == (diamond_rc_t *) NULL) {
	  log_message(LOGT_NET, LOGL_ERR, "device_set_exec_mode: call sending failed");
	  return -1;
	}
	if(rc->service_err != DIAMOND_SUCCESS) {
	  log_message(LOGT_NET, LOGL_ERR, "device_set_exec_mode: call servicing failed");
	  log_message(LOGT_NET, LOGL_ERR, diamond_error(rc));
	  return -1;
	}

	return (0);
}


int
device_set_user_state(void *handle, int id, uint32_t state) 
{
	sdevice_state_t *dev;
	diamond_rc_t *rc;

	dev = (sdevice_state_t *) handle;

	rc = device_set_user_state_x_2(id, state, dev->con_data.tirpc_client);
	if (rc == (diamond_rc_t *) NULL) {
	  log_message(LOGT_NET, LOGL_ERR, "device_set_user_state: call sending failed");
	  return -1;
	}
	if(rc->service_err != DIAMOND_SUCCESS) {
	  log_message(LOGT_NET, LOGL_ERR, "device_set_user_state: call servicing failed");
	  log_message(LOGT_NET, LOGL_ERR, diamond_error(rc));
	  return -1;
	}

	return (0);
}


void
device_stop_obj(void *handle)
{
	sdevice_state_t *dev = (sdevice_state_t *) handle;

	pthread_mutex_lock(&dev->con_data.mutex);
	dev->con_data.flags |= CINFO_BLOCK_OBJ;
	pthread_mutex_unlock(&dev->con_data.mutex);

}

void
device_enable_obj(void *handle)
{
	sdevice_state_t *dev = (sdevice_state_t *) handle;

	pthread_mutex_lock(&dev->con_data.mutex);
	dev->con_data.flags &= ~CINFO_BLOCK_OBJ;
	pthread_mutex_unlock(&dev->con_data.mutex);
}

static void
setup_stats(sdevice_state_t * dev, uint32_t devid)
{
	struct hostent *hent;
	int             len,
	                err;
	char           *delim;
	char            node_name[128];	/* XXX */
	char            path_name[128];	/* XXX */

	hent = gethostbyaddr(&devid, sizeof(devid), AF_INET);
	if (hent == NULL) {
		struct in_addr  in;

		printf("failed to get hostname\n");
		in.s_addr = devid;
		delim = inet_ntoa(in);
		strcpy(node_name, delim);

		/*
		 * replace all the '.' with '_' 
		 */
		while ((delim = index(node_name, '.')) != NULL) {
			*delim = '_';
		}
	} else {
		delim = index(hent->h_name, '.');
		if (delim == NULL) {
			len = strlen(hent->h_name);
		} else {
			len = delim - hent->h_name;
		}
		strncpy(node_name, hent->h_name, len);
		node_name[len] = 0;
	}

	sprintf(path_name, "%s.%s", HOST_NETWORK_PATH, node_name);

	err = dctl_register_node(HOST_NETWORK_PATH, node_name);
	assert(err == 0);

	dctl_register_leaf(path_name, "obj_rx", DCTL_DT_UINT32,
			   dctl_read_uint32, NULL,
			   &dev->con_data.stat_obj_rx);
	dctl_register_leaf(path_name, "obj_total_bytes_rx", DCTL_DT_UINT64,
			   dctl_read_uint64, NULL,
			   &dev->con_data.stat_obj_total_byte_rx);
	dctl_register_leaf(path_name, "obj_hdr_bytes_rx", DCTL_DT_UINT64,
			   dctl_read_uint64, NULL,
			   &dev->con_data.stat_obj_hdr_byte_rx);
	dctl_register_leaf(path_name, "obj_attr_bytes_rx", DCTL_DT_UINT64,
			   dctl_read_uint64, NULL,
			   &dev->con_data.stat_obj_attr_byte_rx);
	dctl_register_leaf(path_name, "obj_data_bytes_rx", DCTL_DT_UINT64,
			   dctl_read_uint64, NULL,
			   &dev->con_data.stat_obj_data_byte_rx);

	dctl_register_leaf(path_name, "control_rx", DCTL_DT_UINT32,
			   dctl_read_uint32, NULL,
			   &dev->con_data.stat_control_rx);
	dctl_register_leaf(path_name, "control_byte_rx", DCTL_DT_UINT64,
			   dctl_read_uint64, NULL,
			   &dev->con_data.stat_control_byte_rx);
	dctl_register_leaf(path_name, "control_tx", DCTL_DT_UINT32,
			   dctl_read_uint32, NULL,
			   &dev->con_data.stat_control_tx);
	dctl_register_leaf(path_name, "control_byte_tx", DCTL_DT_UINT64,
			   dctl_read_uint64, NULL,
			   &dev->con_data.stat_control_byte_tx);
	dctl_register_leaf(path_name, "log_rx", DCTL_DT_UINT32,
			   dctl_read_uint32, NULL,
			   &dev->con_data.stat_log_rx);
	dctl_register_leaf(path_name, "log_byte_rx", DCTL_DT_UINT64,
			   dctl_read_uint64, NULL,
			   &dev->con_data.stat_log_byte_rx);
}

/*
 * This is the initialization function that is
 * called by the searchlet library when we startup.
 */
void *
device_init(int id, uint32_t devid, void *hcookie, hstub_cb_args_t * cb_list,
	    void *dctl_cookie, void *log_cookie)
{
	sdevice_state_t *new_dev;
	int             err;

	new_dev = (sdevice_state_t *) calloc(1, sizeof(*new_dev));
	if (new_dev == NULL) {
		return (NULL);
	}

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
		log_message(LOGT_NET, LOGL_ERR,
		    "device_init: failed to establish connection");
		free(new_dev);
		return (NULL);
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
	new_dev->hstub_conn_down_cb = cb_list->conn_down_cb;


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
			     (void *) new_dev);
	if (err) {
		log_message(LOGT_NET, LOGL_ERR,
		    "device_init: failed to create thread");
		free(new_dev);
		return (NULL);
	}
	return ((void *) new_dev);
}




/*
 * This is used to tear down the state assocaited with the
 * device search.
 */
void
device_fini(sdevice_state_t * dev_state)
{

	free(dev_state);
}
