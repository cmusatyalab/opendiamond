/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 3
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
	diamond_rc_t rc;
	stop_x sx;
	sdevice_state_t *dev;
	enum clnt_stat retval;

	dev = (sdevice_state_t *) handle;

	sx.host_objs_received = hs->hs_objs_received;
	sx.host_objs_queued = hs->hs_objs_queued;
	sx.host_objs_read = hs->hs_objs_read;
	sx.app_objs_queued = hs->hs_objs_uqueued;
	sx.app_objs_presented = hs->hs_objs_upresented;

	memset(&rc, 0, sizeof(rc));
	if(pthread_mutex_lock(&dev->con_data.rpc_mutex) != 0) {
	  log_message(LOGT_NET, LOGL_ERR, "device_stop: couldn't lock mutex");
	  return -1;
	}
	retval = device_stop_x_2(id, sx, &rc, dev->con_data.rpc_client);
	if(pthread_mutex_unlock(&dev->con_data.rpc_mutex) != 0) {
	  log_message(LOGT_NET, LOGL_ERR, "device_stop: couldn't unlock mutex");
	  xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
	  return -1;
	}

	if (retval != RPC_SUCCESS) {
	  log_message(LOGT_NET, LOGL_ERR, "device_stop: call sending failed");
	  log_message(LOGT_NET, LOGL_ERR, clnt_sperrno(retval));
	  xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
	  return -1;
	}
	if(rc.service_err != DIAMOND_SUCCESS) {
	  log_message(LOGT_NET, LOGL_ERR, "device_stop: call servicing failed");
	  log_message(LOGT_NET, LOGL_ERR, diamond_error(&rc));
	  xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
	  return -1;
	}

	xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
	return (0);
}


int
device_terminate(void *handle, int id)
{
	diamond_rc_t rc;
	sdevice_state_t *dev;
	enum clnt_stat retval;

	dev = (sdevice_state_t *) handle;

	memset(&rc, 0, sizeof(rc));
	if(pthread_mutex_lock(&dev->con_data.rpc_mutex) != 0) {
	  log_message(LOGT_NET, LOGL_ERR, "device_terminate: couldn't lock mutex");
	  return -1;
	}
	retval = device_clear_gids_x_2(id, &rc, dev->con_data.rpc_client);
	if(pthread_mutex_unlock(&dev->con_data.rpc_mutex) != 0) {
	  log_message(LOGT_NET, LOGL_ERR, "device_terminate: couldn't unlock mutex");
	  xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
	  return -1;
	}

	if (retval != RPC_SUCCESS) {
	  log_message(LOGT_NET, LOGL_ERR, "device_terminate: call sending failed");
	  log_message(LOGT_NET, LOGL_ERR, clnt_sperrno(retval));
	  xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
	  return -1;
	}
	if(rc.service_err != DIAMOND_SUCCESS) {
	  log_message(LOGT_NET, LOGL_ERR, "device_terminate: call servicing failed");
	  log_message(LOGT_NET, LOGL_ERR, diamond_error(&rc));
	  xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
	  return -1;
	}

	xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
	return (0);
}


/*
 * This start a search that has been setup.  
 */


int
device_start(void *handle, int id)
{
	diamond_rc_t rc;
	sdevice_state_t *dev;
	enum clnt_stat retval;

	dev = (sdevice_state_t *) handle;

	/* save the new start id */
	dev->ver_no = id;

	memset(&rc, 0, sizeof(rc));
	if(pthread_mutex_lock(&dev->con_data.rpc_mutex) != 0) {
	  log_message(LOGT_NET, LOGL_ERR, "device_start: couldn't lock mutex");
	  return -1;
	}
	retval = device_start_x_2(id, &rc, dev->con_data.rpc_client);
	if(pthread_mutex_unlock(&dev->con_data.rpc_mutex) != 0) {
	  log_message(LOGT_NET, LOGL_ERR, "device_start: couldn't unlock mutex");
	  xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
	  return -1;
	}

	if (retval != RPC_SUCCESS) {
	  log_message(LOGT_NET, LOGL_ERR, "device_start: call sending failed");
	  log_message(LOGT_NET, LOGL_ERR, clnt_sperrno(retval));
	  xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
	  return -1;
	}
	if(rc.service_err != DIAMOND_SUCCESS) {
	  log_message(LOGT_NET, LOGL_ERR, "device_start: call servicing failed");
	  log_message(LOGT_NET, LOGL_ERR, diamond_error(&rc));
	  xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
	  return -1;
	}

	xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
	return (0);
}

int
device_clear_gids(void *handle, int id)
{
	diamond_rc_t rc;
	sdevice_state_t *dev;
	enum clnt_stat retval;

	dev = (sdevice_state_t *) handle;

	memset(&rc, 0, sizeof(rc));
	if(pthread_mutex_lock(&dev->con_data.rpc_mutex) != 0) {
	  log_message(LOGT_NET, LOGL_ERR, "device_clear_gids: couldn't lock mutex");
	  return -1;
	}
	retval = device_clear_gids_x_2(id, &rc, dev->con_data.rpc_client);
	if(pthread_mutex_unlock(&dev->con_data.rpc_mutex) != 0) {
	  log_message(LOGT_NET, LOGL_ERR, "device_clear_gids: couldn't unlock mutex");
	  xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
	  return -1;
	}

	if (retval != RPC_SUCCESS) {
	  log_message(LOGT_NET, LOGL_ERR, "device_clear_gids: call sending failed");
	  log_message(LOGT_NET, LOGL_ERR, clnt_sperrno(retval));
	  xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
	  return -1;
	}
	if(rc.service_err != DIAMOND_SUCCESS) {
	  log_message(LOGT_NET, LOGL_ERR, "device_clear_gids: call servicing failed");
	  log_message(LOGT_NET, LOGL_ERR, diamond_error(&rc));
	  xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
	  return -1;
	}

	xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
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
	diamond_rc_t rc;
	sdevice_state_t *dev;
	groupid_x gix;
	enum clnt_stat retval;

	dev = (sdevice_state_t *) handle;

	gix = gid;

	memset(&rc, 0, sizeof(rc));
	if(pthread_mutex_lock(&dev->con_data.rpc_mutex) != 0) {
	  log_message(LOGT_NET, LOGL_ERR, "device_new_gid: couldn't lock mutex");
	  return -1;
	}
	retval = device_new_gid_x_2(id, gix, &rc, dev->con_data.rpc_client);
	if(pthread_mutex_unlock(&dev->con_data.rpc_mutex) != 0) {
	  log_message(LOGT_NET, LOGL_ERR, "device_new_gid: couldn't unlock mutex");
	  xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
	  return -1;
	}

	if (retval != RPC_SUCCESS) {
	  log_message(LOGT_NET, LOGL_ERR, "device_new_gid: call sending failed");
	  log_message(LOGT_NET, LOGL_ERR, clnt_sperrno(retval));
	  xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
	  return -1;
	}
	if(rc.service_err != DIAMOND_SUCCESS) {
	  log_message(LOGT_NET, LOGL_ERR, "device_new_gid: call servicing failed");
	  log_message(LOGT_NET, LOGL_ERR, diamond_error(&rc));
	  xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
	  return -1;
	}

	xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
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
	diamond_rc_t    rc;
	spec_file_x     sf;
	enum clnt_stat retval;

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

	memset(&rc, 0, sizeof(rc));
	if(pthread_mutex_lock(&dev->con_data.rpc_mutex) != 0) {
	  log_message(LOGT_NET, LOGL_ERR, "request_chars: couldn't lock mutex");
	  return -1;
	}
	retval = device_set_spec_x_2(id, sf, &rc, dev->con_data.rpc_client);
	if(pthread_mutex_unlock(&dev->con_data.rpc_mutex) != 0) {
	  log_message(LOGT_NET, LOGL_ERR, "device_set_spec: couldn't unlock mutex");
	  xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
	  return -1;
	}

	if (retval != RPC_SUCCESS) {
	  log_message(LOGT_NET, LOGL_ERR, "device_set_spec: call sending failed");
	  log_message(LOGT_NET, LOGL_ERR, clnt_sperrno(retval));
	  free(data);
	  xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
	  return ENOENT;
	}
	if(rc.service_err != DIAMOND_SUCCESS) {
	  log_message(LOGT_NET, LOGL_ERR, "device_set_spec: call servicing failed");
	  log_message(LOGT_NET, LOGL_ERR, diamond_error(&rc));
	  free(data);
	  xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
	  return ENOENT;
	}
	
	free(data);
	xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);

	return (0);
}

int
device_set_lib(void *handle, int id, sig_val_t *obj_sig)
{
	diamond_rc_t rc;
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
	enum clnt_stat retval;


	dev = (sdevice_state_t *) handle;

	sx.sig_val_x_len = sizeof(sig_val_t);
	sx.sig_val_x_val = (char *)obj_sig;

	memset(&rc, 0, sizeof(rc));
	if(pthread_mutex_lock(&dev->con_data.rpc_mutex) != 0) {
	  log_message(LOGT_NET, LOGL_ERR, "device_set_lib: couldn't lock mutex");
	  return -1;
	}
	retval = device_set_obj_x_2(id, sx, &rc, dev->con_data.rpc_client);
	if(pthread_mutex_unlock(&dev->con_data.rpc_mutex) != 0) {
	  log_message(LOGT_NET, LOGL_ERR, "device_set_lib: couldn't unlock mutex");
	  xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
	  return -1;
	}

	if (retval != RPC_SUCCESS) {
	  log_message(LOGT_NET, LOGL_ERR, "device_set_lib: set_obj call sending failed");
	  log_message(LOGT_NET, LOGL_ERR, clnt_sperrno(retval));
	  xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
	  return -1;
	}
	if(rc.service_err != DIAMOND_SUCCESS) {
	  if(!((rc.service_err == DIAMOND_OPERR) && 
	       (rc.opcode_err == DIAMOND_OPCODE_FCACHEMISS))) {
	    log_message(LOGT_NET, LOGL_ERR, "device_set_lib: call servicing failed");
	    log_message(LOGT_NET, LOGL_ERR, diamond_error(&rc));
	    xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
	    return -1;
	  }
	}
	else { 
	  xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
	  return 0;
	}


	/* If we've reached this point, the server does not have this
	 * filter library and we need to make another send_obj call. */

	cache = dconf_get_binary_cachedir();
	sig = sig_string(obj_sig);
	snprintf(objname, PATH_MAX, OBJ_FORMAT, cache, sig);

	assert(access(objname, F_OK) == 0);

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
		free(data);
		xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
		return (ENOENT);
	}
	if ((rsize = fread(data, buf_len, 1, cur_file)) != 1) {
		log_message(LOGT_NET, LOGL_ERR,
		    "device_set_lib: failed read obj <%s>", objname);
		free(data);
		xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
		return (EAGAIN);
	}

	fclose(cur_file);

	ox.obj_data.obj_data_len = buf_len;
	ox.obj_data.obj_data_val = data;

	xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
	memset(&rc, 0, sizeof(rc));
	if(pthread_mutex_lock(&dev->con_data.rpc_mutex) != 0) {
	  log_message(LOGT_NET, LOGL_ERR, "request_chars: couldn't lock mutex");
	  return -1;
	}
	retval = device_send_obj_x_2(id, ox, &rc, dev->con_data.rpc_client);
	if(pthread_mutex_unlock(&dev->con_data.rpc_mutex) != 0) {
	  log_message(LOGT_NET, LOGL_ERR, "device_set_lib: couldn't unlock mutex");
	  xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
	  return -1;
	}

	if (retval != RPC_SUCCESS) {
	  log_message(LOGT_NET, LOGL_ERR, "device_set_lib: send_obj call sending failed");
	  log_message(LOGT_NET, LOGL_ERR, clnt_sperrno(retval));
	  free(data);
	  xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
	  return -1;
	}
	if(rc.service_err != DIAMOND_SUCCESS) {
	  log_message(LOGT_NET, LOGL_ERR, "device_set_lib: send_obj call servicing failed");
	  log_message(LOGT_NET, LOGL_ERR, diamond_error(&rc));
	  free(data);
	  xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
	  return -1;
	}

	free(data);
	xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);

	return (0);
}



int
device_write_leaf(void *handle, char *path, int len, char *data, int32_t opid)
{
	sdevice_state_t *dev;
	dctl_x          dx;
	int             plen;
	diamond_rc_t    *rc;
	dctl_return_x   drx;
	int             r_err;
	int32_t         r_opid;
	enum clnt_stat retval;


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

	memset(&drx, 0, sizeof(drx));
	if(pthread_mutex_lock(&dev->con_data.rpc_mutex) != 0) {
	  log_message(LOGT_NET, LOGL_ERR, "device_write_leaf: couldn't lock mutex");
	  return -1;
	}
	retval = device_write_leaf_x_2(0, dx, &drx, 
				       dev->con_data.rpc_client);
	if(pthread_mutex_unlock(&dev->con_data.rpc_mutex) != 0) {
	  log_message(LOGT_NET, LOGL_ERR, "device_write_leaf: couldn't unlock mutex");
	  xdr_free((xdrproc_t)xdr_dctl_return_x, (char *)&drx);
	  return -1;
	}

	if (retval != RPC_SUCCESS) {
	  log_message(LOGT_NET, LOGL_ERR, "device_write_leaf: call sending failed");
	  log_message(LOGT_NET, LOGL_ERR, clnt_sperrno(retval));
	  free(data);
	  xdr_free((xdrproc_t)xdr_dctl_return_x, (char *)&drx);
	  return -1;
	}
	rc = &drx.error;
	if(rc->service_err != DIAMOND_SUCCESS) {
	  log_message(LOGT_NET, LOGL_ERR, "device_write_leaf: call servicing failed");
	  log_message(LOGT_NET, LOGL_ERR, diamond_error(rc));
	  free(data);
	  xdr_free((xdrproc_t)xdr_dctl_return_x, (char *)&drx);
	  return -1;
	}

	r_err = drx.dctl.dctl_err;
	r_opid = drx.dctl.dctl_opid;

	(*dev->hstub_wleaf_done_cb) (dev->hcookie, r_err, r_opid);

	free(data);
	xdr_free((xdrproc_t)xdr_dctl_return_x, (char *)&drx);

	return (0);
}

int
device_read_leaf(void *handle, char *path, int32_t opid)
{
	sdevice_state_t *dev;
	dctl_x          dx;
	int             plen;
	diamond_rc_t    *rc;
	dctl_return_x   drx;
	int             r_err;
	int32_t         r_opid;
	int             r_dlen;
	dctl_data_type_t r_dtype;
	enum clnt_stat retval;

	dev = (sdevice_state_t *) handle;

	plen = strlen(path) + 1;

	dx.dctl_err = 0;
	dx.dctl_opid = opid;
	dx.dctl_plen = plen;
	dx.dctl_data.dctl_data_len = plen;
	dx.dctl_data.dctl_data_val = path;

	memset(&drx, 0, sizeof(drx));
	if(pthread_mutex_lock(&dev->con_data.rpc_mutex) != 0) {
	  log_message(LOGT_NET, LOGL_ERR, "device_read_leaf: couldn't lock mutex");
	  return -1;
	}
	retval = device_read_leaf_x_2(0, dx, &drx, dev->con_data.rpc_client);
	if(pthread_mutex_unlock(&dev->con_data.rpc_mutex) != 0) {
	  log_message(LOGT_NET, LOGL_ERR, "device_read_leaf: couldn't unlock mutex");
	  xdr_free((xdrproc_t)xdr_dctl_return_x, (char *)&drx);
	  return -1;
	}

	if (retval != RPC_SUCCESS) {
	  log_message(LOGT_NET, LOGL_ERR, "device_read_leaf: call sending failed");
	  log_message(LOGT_NET, LOGL_ERR, clnt_sperrno(retval));
	  xdr_free((xdrproc_t)xdr_dctl_return_x, (char *)&drx);
	  return -1;
	}
	rc = &drx.error;
	if(rc->service_err != DIAMOND_SUCCESS) {
	  log_message(LOGT_NET, LOGL_ERR, "device_read_leaf: call servicing failed");
	  log_message(LOGT_NET, LOGL_ERR, diamond_error(rc));
	  xdr_free((xdrproc_t)xdr_dctl_return_x, (char *)&drx);
	  return -1;
	}

	r_err = drx.dctl.dctl_err;
	r_opid = drx.dctl.dctl_opid;
	r_dtype = drx.dctl.dctl_dtype;
	r_dlen = drx.dctl.dctl_data.dctl_data_len;

	(*dev->hstub_rleaf_done_cb) (dev->hcookie, r_err, r_dtype, r_dlen,
				     drx.dctl.dctl_data.dctl_data_val, r_opid);

	xdr_free((xdrproc_t)xdr_dctl_return_x, (char *)&drx);

	return (0);

}


int
device_list_nodes(void *handle, char *path, int32_t opid)
{
	sdevice_state_t *dev;
	int             plen;
	dctl_x          dx;
	dctl_return_x   drx;
	int             r_err;
	int32_t         r_opid;
	int             r_dlen;
	diamond_rc_t   *rc;
	int             ents;
	enum clnt_stat retval;

	dev = (sdevice_state_t *) handle;

	plen = strlen(path) + 1;

	dx.dctl_err = 0;
	dx.dctl_opid = opid;
	dx.dctl_plen = plen;
	dx.dctl_data.dctl_data_len = plen;
	dx.dctl_data.dctl_data_val = path;

	memset(&drx, 0, sizeof(drx));
	if(pthread_mutex_lock(&dev->con_data.rpc_mutex) != 0) {
	  log_message(LOGT_NET, LOGL_ERR, "device_list_nodes: couldn't lock mutex");
	  return -1;
	}
	retval = device_list_nodes_x_2(0, dx, &drx,
				       dev->con_data.rpc_client);
	if(pthread_mutex_unlock(&dev->con_data.rpc_mutex) != 0) {
	  log_message(LOGT_NET, LOGL_ERR, "device_list_nodes: couldn't unlock mutex");
	  xdr_free((xdrproc_t)xdr_dctl_return_x, (char *)&drx);
	  return -1;
	}

	if (retval != RPC_SUCCESS) {
	  log_message(LOGT_NET, LOGL_ERR, "device_list_nodes: call sending failed");
	  log_message(LOGT_NET, LOGL_ERR, clnt_sperrno(retval));
	  xdr_free((xdrproc_t)xdr_dctl_return_x, (char *)&drx);
	  return -1;
	}
	rc = &drx.error;
	if(rc->service_err != DIAMOND_SUCCESS) {
	  log_message(LOGT_NET, LOGL_ERR, "device_list_nodes: call servicing failed");
	  log_message(LOGT_NET, LOGL_ERR, diamond_error(rc));
	  xdr_free((xdrproc_t)xdr_dctl_return_x, (char *)&drx);
	  return -1;
	}

	r_err = drx.dctl.dctl_err;
	r_opid = drx.dctl.dctl_opid;
	r_dlen = drx.dctl.dctl_data.dctl_data_len;

	ents = r_dlen / (sizeof(dctl_entry_t));

	(*dev->hstub_lnode_done_cb) (dev->hcookie, r_err, ents,
				     (dctl_entry_t *)drx.dctl.dctl_data.dctl_data_val, r_opid);

	xdr_free((xdrproc_t)xdr_dctl_return_x, (char *)&drx);

	return (0);
}

int
device_list_leafs(void *handle, char *path, int32_t opid)
{
	sdevice_state_t *dev;
	int             plen;
	int             ents;
	dctl_x          dx;
	dctl_return_x   drx;
	int             r_err;
	int32_t         r_opid;
	int             r_dlen;
	diamond_rc_t   *rc;
	enum clnt_stat retval;

	dev = (sdevice_state_t *) handle;

	plen = strlen(path) + 1;

	dx.dctl_err = 0;
	dx.dctl_opid = opid;
	dx.dctl_plen = plen;
	dx.dctl_data.dctl_data_len = plen;
	dx.dctl_data.dctl_data_val = path;

	memset(&drx, 0, sizeof(drx));
	if(pthread_mutex_lock(&dev->con_data.rpc_mutex) != 0) {
	  log_message(LOGT_NET, LOGL_ERR, "device_list_leafs: couldn't lock mutex");
	  return -1;
	}
	retval = device_list_leafs_x_2(0, dx, &drx, 
				       dev->con_data.rpc_client);
	if(pthread_mutex_unlock(&dev->con_data.rpc_mutex) != 0) {
	  log_message(LOGT_NET, LOGL_ERR, "device_list_leafs: couldn't unlock mutex");
	  xdr_free((xdrproc_t)xdr_dctl_return_x, (char *)&drx);
	  return -1;
	}

	if (retval != RPC_SUCCESS) {
	  log_message(LOGT_NET, LOGL_ERR, "device_list_leafs: call sending failed");
	  log_message(LOGT_NET, LOGL_ERR, clnt_sperrno(retval));
	  xdr_free((xdrproc_t)xdr_dctl_return_x, (char *)&drx);
	  return -1;
	}
	rc = &drx.error;
	if(rc->service_err != DIAMOND_SUCCESS) {
	  log_message(LOGT_NET, LOGL_ERR, "device_list_leafs: call servicing failed");
	  log_message(LOGT_NET, LOGL_ERR, diamond_error(rc));
	  xdr_free((xdrproc_t)xdr_dctl_return_x, (char *)&drx);
	  return -1;
	}

	r_err = drx.dctl.dctl_err;
	r_opid = drx.dctl.dctl_opid;
	r_dlen = drx.dctl.dctl_data.dctl_data_len;

	ents = r_dlen / (sizeof(dctl_entry_t));

	(*dev->hstub_lleaf_done_cb) (dev->hcookie, r_err, ents,
				     (dctl_entry_t *)drx.dctl.dctl_data.dctl_data_val, r_opid);

	xdr_free((xdrproc_t)xdr_dctl_return_x, (char *)&drx);

	return (0);
}

int
device_set_blob(void *handle, int id, char *name, int blob_len, void *blob)
{
	sdevice_state_t *dev;
	blob_x           bx;
	diamond_rc_t     rc;
	enum clnt_stat retval;

	dev = (sdevice_state_t *) handle;

	bx.blob_name.blob_name_len = strlen(name) + 1;
	bx.blob_name.blob_name_val = name;
	bx.blob_data.blob_data_len = blob_len;
	bx.blob_data.blob_data_val = blob;

	memset(&rc, 0, sizeof(rc));
	if(pthread_mutex_lock(&dev->con_data.rpc_mutex) != 0) {
	  log_message(LOGT_NET, LOGL_ERR, "device_set_blob: couldn't lock mutex");
	  return -1;
	}
	retval = device_set_blob_x_2(id, bx, &rc, dev->con_data.rpc_client);
	if(pthread_mutex_unlock(&dev->con_data.rpc_mutex) != 0) {
	  log_message(LOGT_NET, LOGL_ERR, "device_set_blob: couldn't unlock mutex");
	  xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
	  return -1;
	}

	if (retval != RPC_SUCCESS) {
	  log_message(LOGT_NET, LOGL_ERR, "device_set_blob: call sending failed");
	  log_message(LOGT_NET, LOGL_ERR, clnt_sperrno(retval));
	  xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
	  return -1;
	}
	if(rc.service_err != DIAMOND_SUCCESS) {
	  log_message(LOGT_NET, LOGL_ERR, "device_set_blob: call servicing failed");
	  log_message(LOGT_NET, LOGL_ERR, diamond_error(&rc));
	  xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
	  return -1;
	}

	xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
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
	diamond_rc_t rc;
	enum clnt_stat retval;

	dev = (sdevice_state_t *) handle;

	memset(&rc, 0, sizeof(rc));
	if(pthread_mutex_lock(&dev->con_data.rpc_mutex) != 0) {
	  log_message(LOGT_NET, LOGL_ERR, "device_set_exec_mode: couldn't lock mutex");
	  return -1;
	}
	retval = device_set_exec_mode_x_2(id, mode, &rc,
					  dev->con_data.rpc_client);
	if(pthread_mutex_unlock(&dev->con_data.rpc_mutex) != 0) {
	  log_message(LOGT_NET, LOGL_ERR, "device_set_exec_mode: couldn't unlock mutex");
	  xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
	  return -1;
	}

	if (retval != RPC_SUCCESS) {
	  log_message(LOGT_NET, LOGL_ERR, "device_set_exec_mode: call sending failed");
	  log_message(LOGT_NET, LOGL_ERR, clnt_sperrno(retval));
	  xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
	  return -1;
	}
	if(rc.service_err != DIAMOND_SUCCESS) {
	  log_message(LOGT_NET, LOGL_ERR, "device_set_exec_mode: call servicing failed");
	  log_message(LOGT_NET, LOGL_ERR, diamond_error(&rc));
	  xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
	  return -1;
	}

	xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
	return (0);
}


int
device_set_user_state(void *handle, int id, uint32_t state) 
{
	sdevice_state_t *dev;
	diamond_rc_t     rc;
	enum clnt_stat retval;

	dev = (sdevice_state_t *) handle;

	memset(&rc, 0, sizeof(rc));
	if(pthread_mutex_lock(&dev->con_data.rpc_mutex) != 0) {
	  log_message(LOGT_NET, LOGL_ERR, "device_set_user_state: couldn't lock mutex");
	  return -1;
	}
	retval = device_set_user_state_x_2(id, state, &rc, 
					   dev->con_data.rpc_client);
	if(pthread_mutex_unlock(&dev->con_data.rpc_mutex) != 0) {
	  log_message(LOGT_NET, LOGL_ERR, "device_set_user_state: couldn't unlock mutex");
	  xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
	  return -1;
	}

	if (retval != RPC_SUCCESS) {
	  log_message(LOGT_NET, LOGL_ERR, "device_set_user_state: call sending failed");
	  log_message(LOGT_NET, LOGL_ERR, clnt_sperrno(retval));
	  xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
	  return -1;
	}
	if(rc.service_err != DIAMOND_SUCCESS) {
	  log_message(LOGT_NET, LOGL_ERR, "device_set_user_state: call servicing failed");
	  log_message(LOGT_NET, LOGL_ERR, diamond_error(&rc));
	  xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
	  return -1;
	}

	xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
	return (0);
}

int
device_get_session_variables(void *handle, device_session_vars_t **vars)
{
  sdevice_state_t *dev = (sdevice_state_t *) handle;
  diamond_session_var_list_return_x rc;
  enum clnt_stat retval;

  memset(&rc, 0, sizeof(rc));
  if(pthread_mutex_lock(&dev->con_data.rpc_mutex) != 0) {
    log_message(LOGT_NET, LOGL_ERR, "device_get_session_variables: couldn't lock mutex");
    return -1;
  }
  retval = session_variables_get_x_2(0, &rc, dev->con_data.rpc_client);
  if(pthread_mutex_unlock(&dev->con_data.rpc_mutex) != 0) {
    log_message(LOGT_NET, LOGL_ERR, "device_get_session_variables: couldn't unlock mutex");
    return -1;
  }


  if (retval != RPC_SUCCESS) {
    log_message(LOGT_NET, LOGL_ERR, "device_get_session_variables: call sending failed");
    log_message(LOGT_NET, LOGL_ERR, clnt_spcreateerror("device_get_session_variables"));
    return -1;
  }

  if(rc.error.service_err != DIAMOND_SUCCESS) {
    log_message(LOGT_NET, LOGL_ERR, "device_get_session_variables: call servicing failed");
    log_message(LOGT_NET, LOGL_ERR, diamond_error(&rc.error));
    return -1;
  }

  // allocate
  *vars = calloc(1, sizeof(device_session_vars_t));
  if (*vars == NULL) {
    log_message(LOGT_NET, LOGL_ERR, "device_get_session_variables: no memory");
    return -1;
  }

  // count length
  int len = 0;
  diamond_session_var_list_x *first = rc.l;
  diamond_session_var_list_x *cur = first;

  while (cur != NULL) {
    cur = cur->next;
    len++;
  }

  // allocate some more
  (*vars)->len = len;
  (*vars)->names = calloc(len, sizeof(char *));
  (*vars)->values = calloc(len, sizeof(double));

  // copy
  int i = 0;
  cur = first;
  while (cur != NULL) {
    (*vars)->names[i] = strdup(cur->name);
    (*vars)->values[i] = cur->value;

    cur = cur->next;
    i++;
  }

  // deallocate old
  cur = first;
  while (cur != NULL) {
    diamond_session_var_list_x *prev = cur;
    free(cur->name);
    cur = cur->next;
    free(prev);
  }
  rc.l = NULL;

  return 0;
}

int
device_set_session_variables(void *handle, device_session_vars_t *vars)
{
  if (vars->len == 0) {
    return 0;
  }

  sdevice_state_t *dev = (sdevice_state_t *) handle;
  diamond_rc_t rc;
  enum clnt_stat retval;

  // create list
  diamond_session_var_list_x *first = NULL;
  diamond_session_var_list_x *prev = NULL;

  int i;
  for (i = 0; i < vars->len; i++) {
    diamond_session_var_list_x *l = calloc(1, sizeof(diamond_session_var_list_x));
    if (l == NULL) {
      break;
    }

    if (i == 0) {
      first = l;
    } else {
      prev->next = l;
    }

    // load values
    l->name = vars->names[i];
    l->value = vars->values[i];

    //printf(" device_set_session_variables %d: \"%s\" -> %g\n", i, l->name, l->value);

    prev = l;
  }

  memset(&rc, 0, sizeof(rc));
  if(pthread_mutex_lock(&dev->con_data.rpc_mutex) != 0) {
    log_message(LOGT_NET, LOGL_ERR, "device_set_session_variables: couldn't lock mutex");
    return -1;
  }
  retval = session_variables_set_x_2(0, *first, &rc, dev->con_data.rpc_client);
  if(pthread_mutex_unlock(&dev->con_data.rpc_mutex) != 0) {
    log_message(LOGT_NET, LOGL_ERR, "device_set_session_variables: couldn't unlock mutex");
    return -1;
  }

  // free
  diamond_session_var_list_x *cur = first;
  while (cur != NULL) {
    prev = cur;
    cur = cur->next;
    free(prev);
  }


  if (retval != RPC_SUCCESS) {
    log_message(LOGT_NET, LOGL_ERR, "device_set_session_variables: call sending failed");
    log_message(LOGT_NET, LOGL_ERR, clnt_spcreateerror("device_set_session_variables"));
    return -1;
  }

  if(rc.service_err != DIAMOND_SUCCESS) {
    log_message(LOGT_NET, LOGL_ERR, "device_set_session_variables: call servicing failed");
    log_message(LOGT_NET, LOGL_ERR, diamond_error(&rc));
    return -1;
  }

  return 0;
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
device_init(int id, uint32_t devid, void *hcookie, hstub_cb_args_t * cb_list)
{
	sdevice_state_t *new_dev;
	int             err;

	new_dev = (sdevice_state_t *) calloc(1, sizeof(*new_dev));
	if (new_dev == NULL) {
		return (NULL);
	}

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

	err = pthread_create(&new_dev->thread_id, NULL, hstub_main,
			     (void *) new_dev);
	if (err) {
		log_message(LOGT_NET, LOGL_ERR,
		    "device_init: failed to create thread");
		free(new_dev);
		return (NULL);
	}
	return ((void *) new_dev);
}
