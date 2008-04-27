/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 3
 *
 *  Copyright (c) 2002-2007 Intel Corporation
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
 *  Copyright (c) 2008 Carnegie Mellon University
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
#include "lib_dconfig.h"
#include "hstub_impl.h"
#include "rpc_client_content.h"
#include "rpc_preamble.h"

/*
 * XXX move to common header 
 */
#define	HSTUB_RING_SIZE	512
#define OBJ_RING_SIZE	512

int rpc_preproc(const char *func, struct conn_info *con)
{
	if(pthread_mutex_lock(&con->rpc_mutex) != 0) {
		log_message(LOGT_NET, LOGL_ERR, "%s: mutex lock failed", func);
		return -1;
	}
	return 0;
}

int rpc_postproc(const char *func, struct conn_info *con,
		 enum clnt_stat rpc_rc, diamond_rc_t *rc)
{
	if (pthread_mutex_unlock(&con->rpc_mutex) != 0) {
		log_message(LOGT_NET, LOGL_ERR, "%s: mutex unlock failed",func);
		return -1;
	}
	if (rpc_rc != RPC_SUCCESS) {
		log_message(LOGT_NET, LOGL_ERR, "%s: sending failed", func);
		log_message(LOGT_NET, LOGL_ERR, clnt_sperrno(rpc_rc));
		return -1;
	}
	if (rc->service_err != DIAMOND_SUCCESS) {
		log_message(LOGT_NET, LOGL_ERR, "%s: servicing failed", func);
		log_message(LOGT_NET, LOGL_ERR, diamond_error(rc));
		return -1;
	}
	return 0;
}

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
	int err;

	dev = (sdevice_state_t *) handle;

	sx.host_objs_received = hs->hs_objs_received;
	sx.host_objs_queued = hs->hs_objs_queued;
	sx.host_objs_read = hs->hs_objs_read;
	sx.app_objs_queued = hs->hs_objs_uqueued;
	sx.app_objs_presented = hs->hs_objs_upresented;

	memset(&rc, 0, sizeof(rc));
	if (rpc_preproc(__FUNCTION__, &dev->con_data))
		return -1;

	retval = device_stop_x_2(id, sx, &rc, dev->con_data.rpc_client);

	err = rpc_postproc(__FUNCTION__, &dev->con_data, retval, &rc);
	xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
	return err;
}


int
device_terminate(void *handle, int id)
{
	diamond_rc_t rc;
	sdevice_state_t *dev;
	enum clnt_stat retval;
	int err;

	dev = (sdevice_state_t *) handle;

	memset(&rc, 0, sizeof(rc));
	if (rpc_preproc(__FUNCTION__, &dev->con_data))
		return -1;

	retval = device_clear_gids_x_2(id, &rc, dev->con_data.rpc_client);

	err = rpc_postproc(__FUNCTION__, &dev->con_data, retval, &rc);
	xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
	return err;
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
	int err;

	dev = (sdevice_state_t *) handle;

	/* save the new start id */
	dev->ver_no = id;

	memset(&rc, 0, sizeof(rc));
	if (rpc_preproc(__FUNCTION__, &dev->con_data))
		return -1;

	retval = device_start_x_2(id, &rc, dev->con_data.rpc_client);

	err = rpc_postproc(__FUNCTION__, &dev->con_data, retval, &rc);
	xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
	return err;
}

int
device_clear_gids(void *handle, int id)
{
	diamond_rc_t rc;
	sdevice_state_t *dev;
	enum clnt_stat retval;
	int err;

	dev = (sdevice_state_t *) handle;

	memset(&rc, 0, sizeof(rc));
	if (rpc_preproc(__FUNCTION__, &dev->con_data))
		return -1;

	retval = device_clear_gids_x_2(id, &rc, dev->con_data.rpc_client);

	err = rpc_postproc(__FUNCTION__, &dev->con_data, retval, &rc);
	xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
	return err;
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
	int err;

	dev = (sdevice_state_t *) handle;

	gix = gid;

	memset(&rc, 0, sizeof(rc));
	if (rpc_preproc(__FUNCTION__, &dev->con_data))
		return -1;

	retval = device_new_gid_x_2(id, gix, &rc, dev->con_data.rpc_client);

	err = rpc_postproc(__FUNCTION__, &dev->con_data, retval, &rc);
	xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
	return err;
}


/*
 * This builds the command to set the searchlet on the remote device.
 * This builds the buffers and copies the contents of the files into
 * the buffers.
 */


int
device_set_spec(void *handle, int id, char *spec, sig_val_t *sig)
{
	char           *data;
	int             spec_len;
	struct stat     stats;
	ssize_t         rsize;
	FILE           *cur_file;
	sdevice_state_t *dev;
	diamond_rc_t    rc;
	spec_file_x     sf;
	enum clnt_stat retval;
	int err;

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
	if (rpc_preproc(__FUNCTION__, &dev->con_data)) {
		free(data);
		return -1;
	}

	retval = device_set_spec_x_2(id, sf, &rc, dev->con_data.rpc_client);

	err = rpc_postproc(__FUNCTION__, &dev->con_data, retval, &rc);
	xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
	free(data);
	return err;
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
	int		buf_len;
	FILE           *cur_file;
	char objname[PATH_MAX];
	char *cache;
	char *data;
	char *sig;
	enum clnt_stat retval;
	int err, cachemiss;

	dev = (sdevice_state_t *) handle;

	sx.sig_val_x_len = sizeof(sig_val_t);
	sx.sig_val_x_val = (char *)obj_sig;

	memset(&rc, 0, sizeof(rc));
	if (rpc_preproc(__FUNCTION__, &dev->con_data))
		return -1;

	retval = device_set_obj_x_2(id, sx, &rc, dev->con_data.rpc_client);

	/* cache misses are not really errors, we don't need them logged */
	cachemiss = (retval == RPC_SUCCESS &&
		     rc.service_err == DIAMOND_OPERR &&
		     rc.opcode_err == DIAMOND_OPCODE_FCACHEMISS);
	if (cachemiss) {
		rc.service_err = DIAMOND_SUCCESS;
		rc.opcode_err = DIAMOND_OPCODE_SUCCESS;
	}

	err = rpc_postproc(__FUNCTION__, &dev->con_data, retval, &rc);
	xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
	if (err || !cachemiss) return err;

	/* If we've reached this point, the server does not have this
	 * filter library and we need to make another send_obj call. */

	cache = dconf_get_binary_cachedir();
	sig = sig_string(obj_sig);
	snprintf(objname, PATH_MAX, SO_FORMAT, cache, sig);

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
		return (ENOENT);
	}
	if ((rsize = fread(data, buf_len, 1, cur_file)) != 1) {
		log_message(LOGT_NET, LOGL_ERR,
		    "device_set_lib: failed read obj <%s>", objname);
		free(data);
		return (EAGAIN);
	}

	fclose(cur_file);

	ox.obj_data.obj_data_len = buf_len;
	ox.obj_data.obj_data_val = data;

	memset(&rc, 0, sizeof(rc));
	if (rpc_preproc(__FUNCTION__, &dev->con_data)) {
		free(data);
		return -1;
	}

	retval = device_send_obj_x_2(id, ox, &rc, dev->con_data.rpc_client);

	err = rpc_postproc(__FUNCTION__, &dev->con_data, retval, &rc);
	xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
	free(data);
	return err;
}



int
device_write_leaf(void *handle, char *path, int len, char *data)
{
	sdevice_state_t *dev;
	dctl_x          dx;
	int             plen;
	dctl_return_x   drx;
	enum clnt_stat retval;
	char *buf;
	int err;

	dev = (sdevice_state_t *) handle;

	plen = strlen(path) + 1;

	if((buf = malloc(plen+len)) == NULL) {
	  log_message(LOGT_NET, LOGL_ERR,
		      "device_write_leaf: failed malloc data");
	  return (EAGAIN);
	}

	memcpy(&buf[0], path, plen);
	memcpy(&buf[plen], data, len);

	dx.dctl_err = 0;
	dx.dctl_opid = 0;
	dx.dctl_plen = plen;
	dx.dctl_data.dctl_data_len = plen+len;
	dx.dctl_data.dctl_data_val = buf;

	memset(&drx, 0, sizeof(drx));
	if (rpc_preproc(__FUNCTION__, &dev->con_data)) {
		free(buf);
		return -1;
	}

	retval = device_write_leaf_x_2(0, dx, &drx, dev->con_data.rpc_client);

	err = rpc_postproc(__FUNCTION__, &dev->con_data, retval, &drx.error);
	free(buf);
	if (err) {
		xdr_free((xdrproc_t)xdr_dctl_return_x, (char *)&drx);
		return -1;
	}
	err = drx.dctl.dctl_err;
	xdr_free((xdrproc_t)xdr_dctl_return_x, (char *)&drx);
	return err;
}

int
device_read_leaf(void *handle, char *path,
		 dctl_data_type_t *dtype, int *dlen, void *dval)
{
	sdevice_state_t *dev;
	dctl_x          dx;
	int             len;
	dctl_return_x   drx;
	enum clnt_stat retval;
	int err;

	dev = (sdevice_state_t *) handle;

	len = strlen(path) + 1;

	dx.dctl_err = 0;
	dx.dctl_opid = 0;
	dx.dctl_plen = len;
	dx.dctl_data.dctl_data_len = len;
	dx.dctl_data.dctl_data_val = path;

	memset(&drx, 0, sizeof(drx));
	if (rpc_preproc(__FUNCTION__, &dev->con_data))
		return -1;

	retval = device_read_leaf_x_2(0, dx, &drx, dev->con_data.rpc_client);

	err = rpc_postproc(__FUNCTION__, &dev->con_data, retval, &drx.error);
	if (err) {
		xdr_free((xdrproc_t)xdr_dctl_return_x, (char *)&drx);
		return -1;
	}
	err = drx.dctl.dctl_err;
	len = drx.dctl.dctl_data.dctl_data_len;
	if (len <= *dlen)
		memcpy(dval, drx.dctl.dctl_data.dctl_data_val, len);
	else
		err = ENOMEM;

	*dlen = len;
	*dtype = drx.dctl.dctl_dtype;

	xdr_free((xdrproc_t)xdr_dctl_return_x, (char *)&drx);
	return err;
}


int
device_list_nodes(void *handle, char *path, int *dents, dctl_entry_t *dval)
{
	sdevice_state_t *dev;
	int		len, ents;
	dctl_x		dx;
	dctl_return_x   drx;
	enum clnt_stat retval;
	int err;

	dev = (sdevice_state_t *) handle;

	len = strlen(path) + 1;

	dx.dctl_err = 0;
	dx.dctl_opid = 0;
	dx.dctl_plen = len;
	dx.dctl_data.dctl_data_len = len;
	dx.dctl_data.dctl_data_val = path;

	memset(&drx, 0, sizeof(drx));
	if (rpc_preproc(__FUNCTION__, &dev->con_data))
		return -1;

	retval = device_list_nodes_x_2(0, dx, &drx, dev->con_data.rpc_client);

	err = rpc_postproc(__FUNCTION__, &dev->con_data, retval, &drx.error);
	if (err) {
		xdr_free((xdrproc_t)xdr_dctl_return_x, (char *)&drx);
		return -1;
	}

	err = drx.dctl.dctl_err;
	ents = drx.dctl.dctl_data.dctl_data_len / sizeof(*dval);
	if (ents <= *dents)
		memcpy(dval, drx.dctl.dctl_data.dctl_data_val,
		       ents * sizeof(*dval));
	else
		err = ENOMEM;

	*dents = ents;

	xdr_free((xdrproc_t)xdr_dctl_return_x, (char *)&drx);
	return 0;
}

int
device_list_leafs(void *handle, char *path, int *dents, dctl_entry_t *dval)
{
	sdevice_state_t *dev;
	int		len, ents;
	dctl_x		dx;
	dctl_return_x   drx;
	enum clnt_stat retval;
	int err;

	dev = (sdevice_state_t *) handle;

	len = strlen(path) + 1;

	dx.dctl_err = 0;
	dx.dctl_opid = 0;
	dx.dctl_plen = len;
	dx.dctl_data.dctl_data_len = len;
	dx.dctl_data.dctl_data_val = path;

	memset(&drx, 0, sizeof(drx));
	if (rpc_preproc(__FUNCTION__, &dev->con_data))
		return -1;

	retval = device_list_leafs_x_2(0, dx, &drx, dev->con_data.rpc_client);

	err = rpc_postproc(__FUNCTION__, &dev->con_data, retval, &drx.error);
	if (err) {
		xdr_free((xdrproc_t)xdr_dctl_return_x, (char *)&drx);
		return -1;
	}

	err = drx.dctl.dctl_err;
	ents = drx.dctl.dctl_data.dctl_data_len / sizeof(*dval);
	if (ents <= *dents)
		memcpy(dval, drx.dctl.dctl_data.dctl_data_val,
		       ents * sizeof(*dval));
	else
		err = ENOMEM;

	*dents = ents;

	xdr_free((xdrproc_t)xdr_dctl_return_x, (char *)&drx);
	return 0;
}

int
device_set_blob(void *handle, int id, char *name, int blob_len, void *blob)
{
	sdevice_state_t *dev;
	blob_x           bx;
	diamond_rc_t     rc;
	enum clnt_stat retval;
	int err;

	dev = (sdevice_state_t *) handle;

	bx.blob_name.blob_name_len = strlen(name) + 1;
	bx.blob_name.blob_name_val = name;
	bx.blob_data.blob_data_len = blob_len;
	bx.blob_data.blob_data_val = blob;

	memset(&rc, 0, sizeof(rc));
	if (rpc_preproc(__FUNCTION__, &dev->con_data))
		return -1;

	retval = device_set_blob_x_2(id, bx, &rc, dev->con_data.rpc_client);

	err = rpc_postproc(__FUNCTION__, &dev->con_data, retval, &rc);
	xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
	return err;
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
	int err;

	dev = (sdevice_state_t *) handle;

	memset(&rc, 0, sizeof(rc));
	if (rpc_preproc(__FUNCTION__, &dev->con_data))
		return -1;

	retval = device_set_exec_mode_x_2(id, mode, &rc,
					  dev->con_data.rpc_client);

	err = rpc_postproc(__FUNCTION__, &dev->con_data, retval, &rc);
	xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
	return err;
}


int
device_set_user_state(void *handle, int id, uint32_t state) 
{
	sdevice_state_t *dev;
	diamond_rc_t     rc;
	enum clnt_stat retval;
	int err;

	dev = (sdevice_state_t *) handle;

	memset(&rc, 0, sizeof(rc));
	if (rpc_preproc(__FUNCTION__, &dev->con_data))
		return -1;

	retval = device_set_user_state_x_2(id, state, &rc,
					   dev->con_data.rpc_client);

	err = rpc_postproc(__FUNCTION__, &dev->con_data, retval, &rc);
	xdr_free((xdrproc_t)xdr_diamond_rc_t, (char *)&rc);
	return err;
}

int
device_get_session_variables(void *handle, device_session_vars_t **vars)
{
	sdevice_state_t *dev = (sdevice_state_t *) handle;
	diamond_session_var_list_return_x rc;
	enum clnt_stat retval;
	int err;

	*vars = NULL;
	memset(&rc, 0, sizeof(rc));
	if (rpc_preproc(__FUNCTION__, &dev->con_data))
		return -1;

	retval = session_variables_get_x_2(0, &rc, dev->con_data.rpc_client);

	err = rpc_postproc(__FUNCTION__, &dev->con_data, retval, &rc.error);
	if (err) goto err_out;

	// allocate
	*vars = calloc(1, sizeof(device_session_vars_t));
	if (*vars == NULL) {
		log_message(LOGT_NET, LOGL_ERR,
			    "device_get_session_variables: no memory");
		err = -1;
		goto err_out;
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
err_out:
	xdr_free((xdrproc_t)xdr_diamond_session_var_list_return_x, (char *)&rc);
	return err;
}

int
device_set_session_variables(void *handle, device_session_vars_t *vars)
{
	sdevice_state_t *dev = (sdevice_state_t *) handle;
	diamond_rc_t rc;
	enum clnt_stat retval;
	diamond_session_var_list_x *first = NULL;
	diamond_session_var_list_x *prev = NULL;
	int i, err;

	if (vars->len == 0) return 0;

	for (i = 0; i < vars->len; i++) {
		diamond_session_var_list_x *l =
			calloc(1, sizeof(diamond_session_var_list_x));
		if (l == NULL) break;

		if (i == 0) {
			first = l;
		} else {
			prev->next = l;
		}

		// load values
		l->name = vars->names[i];
		l->value = vars->values[i];

		//printf(" device_set_session_variables %d: \"%s\" -> %g\n",
		//	 i, l->name, l->value);

		prev = l;
	}

	memset(&rc, 0, sizeof(rc));
	err = rpc_preproc(__FUNCTION__, &dev->con_data);
	if (err) goto err_out;

	retval = session_variables_set_x_2(0, *first, &rc,
					   dev->con_data.rpc_client);

	err = rpc_postproc(__FUNCTION__, &dev->con_data, retval, &rc);

err_out:
	while (first != NULL) {
		prev = first;
		first = first->next;
		free(prev);
	}
	return err;
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
setup_stats(sdevice_state_t * dev, const char *host)
{
	int 		err;
	char           *node_name, *delim;
	char            path_name[128];	/* XXX */

	node_name = strdup(host);

	/*
	 * replace all the '.' with '_'
	 */
	while ((delim = index(node_name, '.')) != NULL)
		*delim = '_';

	snprintf(path_name, 128, "%s.%s", HOST_NETWORK_PATH, node_name);

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
	free(node_name);
}

/*
 * This is the initialization function that is
 * called by the searchlet library when we startup.
 */
void *
device_init(int id, const char *host, void *hcookie, hstub_cb_args_t * cb_list)
{
	sdevice_state_t *new_dev;
	int             err;

	new_dev = (sdevice_state_t *) calloc(1, sizeof(*new_dev));
	if (new_dev == NULL) {
		return (NULL);
	}

	err = ring_init(&new_dev->obj_ring, OBJ_RING_SIZE);
	if (err) {
		free(new_dev);
		return (NULL);
	}


	pthread_mutex_init(&new_dev->con_data.mutex, NULL);

	/*
	 * Open the sockets to the new host.
	 */
	err = hstub_establish_connection(&new_dev->con_data, host);
	if (err) {
		log_message(LOGT_NET, LOGL_ERR,
		    "device_init: failed to establish connection");
		free(new_dev);
		return (NULL);
	}

	/* Save the callback and the host cookie. */
	new_dev->hcookie = hcookie;
	new_dev->cb = *cb_list;

	/*
	 * Init caches stats.
	 */
	new_dev->dstats = NULL;
	new_dev->stat_size = 0;

	setup_stats(new_dev, host);

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
