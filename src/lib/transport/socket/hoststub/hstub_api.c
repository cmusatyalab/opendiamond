/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 4
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
#include "dctl_common.h"
#include "lib_hstub.h"
#include "lib_dconfig.h"
#include "hstub_impl.h"
#include "dconfig_priv.h"
#include "dctl_impl.h"
#include "odisk_priv.h"

#include "rpc_client_content_client.h"

/*
 * XXX move to common header 
 */
#define	HSTUB_RING_SIZE	512
#define OBJ_RING_SIZE	512

int rpc_postproc(const char *func, mrpc_status_t ret)
{
	if (ret < 0) { /* minirpc error */
		log_message(LOGT_NET, LOGL_ERR, "%s: sending failed", func);
		log_message(LOGT_NET, LOGL_ERR, mrpc_strerror(ret));
		return -1;
	}
	else if (ret > 0) { /* diamond error */
		log_message(LOGT_NET, LOGL_ERR, "%s: servicing failed", func);
		log_message(LOGT_NET, LOGL_ERR, diamond_error(ret));
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
device_stop(void *handle, host_stats_t *hs)
{
	stop_x		sx;
	sdevice_state_t *dev;
	mrpc_status_t	retval;

	dev = (sdevice_state_t *) handle;

	sx.host_objs_received = hs->hs_objs_received;
	sx.host_objs_queued = hs->hs_objs_queued;
	sx.host_objs_read = hs->hs_objs_read;
	sx.app_objs_queued = hs->hs_objs_uqueued;
	sx.app_objs_presented = hs->hs_objs_upresented;

	retval = rpc_client_content_device_stop(dev->con_data.rpc_client, &sx);

	return rpc_postproc(__FUNCTION__, retval);
}


int
device_terminate(void *handle)
{
	sdevice_state_t *dev;
	mrpc_status_t	retval;

	dev = (sdevice_state_t *) handle;

	retval = rpc_client_content_device_terminate(dev->con_data.rpc_client);

	/* mark the connection as closing/closed */
	mrpc_conn_close(dev->con_data.rpc_client);

	return rpc_postproc(__FUNCTION__, retval);
}


/*
 * This start a search that has been setup.  
 */
int
device_start(void *handle, unsigned int search_id)
{
	sdevice_state_t *dev;
	mrpc_status_t	retval;
	start_x		sx;

	dev = (sdevice_state_t *) handle;

	sx.search_id = dev->search_id = search_id;

	retval = rpc_client_content_device_start(dev->con_data.rpc_client, &sx);

	return rpc_postproc(__FUNCTION__, retval);
}

int
device_clear_gids(void *handle)
{
	sdevice_state_t *dev;
	mrpc_status_t	retval;

	dev = (sdevice_state_t *) handle;

	retval = rpc_client_content_device_clear_gids(dev->con_data.rpc_client);
	return rpc_postproc(__FUNCTION__, retval);
}


obj_data_t *
device_next_obj(void *handle)
{
	sdevice_state_t *dev;
	obj_data_t     *obj;

	dev = (sdevice_state_t *) handle;
	obj = ring_deq(dev->obj_ring);

	if (obj != NULL) {
		dev->con_data.flags |= CINFO_PENDING_CREDIT;
	}
	else if (dev->con_data.cc_counter++ > 100) {
		dev->con_data.flags |= CINFO_PENDING_CREDIT;
		dev->con_data.cc_counter = 0;
	}
	return obj;
}

void
device_drain_objs(void *handle)
{
	sdevice_state_t *dev = (sdevice_state_t *) handle;
	obj_data_t     *obj;

	/* prevent addition of more objects to the ring. */
	dev->search_id = 0;

	while ((obj = ring_deq(dev->obj_ring)) != NULL)
		odisk_release_obj(obj);

	dev->con_data.flags |= CINFO_PENDING_CREDIT;
}

int
device_new_gid(void *handle, groupid_t gid)
{
	sdevice_state_t *dev;
	groupid_x	x;
	mrpc_status_t	retval;

	dev = (sdevice_state_t *) handle;
	x = gid;

	retval = rpc_client_content_device_new_gid(dev->con_data.rpc_client,&x);
	return rpc_postproc(__FUNCTION__, retval);
}


/*
 * This builds the command to set the searchlet on the remote device.
 * This builds the buffers and copies the contents of the files into
 * the buffers.
 */
int
device_set_spec(void *handle, char *spec, sig_val_t *sig)
{
	char		*data = NULL;
	int		spec_len;
	struct stat	stats;
	ssize_t		rsize;
	FILE		*cur_file;
	sdevice_state_t *dev;
	spec_file_x	sf;
	mrpc_status_t	retval;
	int		err;

	dev = (sdevice_state_t *) handle;

	err = stat(spec, &stats);
	if (err) {
	    log_message(LOGT_NET, LOGL_ERR,
			"device_set_searchlet: failed stat spec file <%s>",
			spec);
	    goto err_out;
	}

	err = -1;
	spec_len = stats.st_size;
	data = malloc(spec_len);
	if (data == NULL) {
	    log_message(LOGT_NET, LOGL_ERR,
			"device_set_searchlet: failed open spec <%s>", spec);
	    goto err_out;
	}

	/*
	 * set data to the beginning of the data portion  and
	 * copy in the filter spec from the file.  NOTE: This is
	 * currently blocks, we may want to do something else later.
	 */
	cur_file = fopen(spec, "r");
	if (cur_file == NULL) {
	    log_message(LOGT_NET, LOGL_ERR,
			"device_set_searchlet: failed open spec <%s>", spec);
	    goto err_out;
	}
	rsize = fread(data, spec_len, 1, cur_file);
	if (rsize != 1) {
	    log_message(LOGT_NET, LOGL_ERR,
			"device_set_searchlet: failed read spec <%s>",
			spec);
	    goto err_out;
	}

	fclose(cur_file);

	sf.sig.sig_val_x_len = sizeof(sig_val_t);
	sf.sig.sig_val_x_val = (char *)sig;

	sf.data.data_len = spec_len;
	sf.data.data_val = data;

	retval = rpc_client_content_device_set_spec
					(dev->con_data.rpc_client, &sf);
	err = rpc_postproc(__FUNCTION__, retval);
err_out:
	if (data) free(data);
	return err;
}

int
device_set_thumbnail_attrs(void *handle, const char **attrs)
{
	sdevice_state_t *dev;
	attr_name_list_x req;
	int		n = 0;
	mrpc_status_t	retval;
	int		err;

	dev = (sdevice_state_t *) handle;

	/* count nr. of attribute names */
	while (attrs[n] != NULL) n++;

	req.attrs.attrs_len = n;
	req.attrs.attrs_val = malloc(n * sizeof(attr_name_x));
	if (req.attrs.attrs_val == NULL)
		return -1;
	for (n = 0; attrs[n] != NULL; n++)
		req.attrs.attrs_val[n] = (char *)attrs[n];

	retval = rpc_client_content_device_set_thumbnail_attrs
					(dev->con_data.rpc_client, &req);
	err = rpc_postproc(__FUNCTION__, retval);

	dev->thumbnails = (err == 0);

	free(req.attrs.attrs_val);
	return err;
}

int
device_reexecute_filters(void *handle, obj_data_t *obj, const char **attrs)
{
	sdevice_state_t *dev = (sdevice_state_t *) handle;
	attribute_list_x *res = NULL;
	reexecute_x	req;
	unsigned int	i = 0;
	mrpc_status_t	retval;
	int		err;

	/* obj->data should be a null terminated string */
	if (!obj->data_len || ((char *)obj->data)[obj->data_len-1] != '\0')
		return 0;
	req.object_id = obj->data;

	/* count nr. of attribute names */
	while (attrs[i] != NULL) i++;

	req.attrs.attrs_len = i;
	req.attrs.attrs_val = malloc(i * sizeof(attr_name_x));
	if (req.attrs.attrs_val == NULL)
		return -1;

	for (i = 0; attrs[i] != NULL; i++)
		req.attrs.attrs_val[i] = (char *)attrs[i];

	retval = rpc_client_content_device_reexecute_filters
					(dev->con_data.rpc_client, &req, &res);
	err = rpc_postproc(__FUNCTION__, retval);
	if (err) goto err_out;

	for (i = 0; i < res->attrs.attrs_len; i++)
	{
		attribute_x *in = &res->attrs.attrs_val[i];

		err = obj_write_attr(&obj->attr_info, in->name,
				     in->data.data_len,
				     (unsigned char *)in->data.data_val);
		if (err) {
			log_message(LOGT_NET, LOGL_CRIT,
				    "reexecute_filter: obj_write_attr failed");
			break;
		}
	}

err_out:
	free_attribute_list_x(res, 1);
	free(req.attrs.attrs_val);
	return err;
}


int
device_set_lib(void *handle, sig_val_t *obj_sig)
{
	char		*data = NULL;
	sdevice_state_t *dev;
	sig_val_x	sx;
	obj_x		ox;
	struct stat     stats;
	ssize_t		rsize;
	int		buf_len;
	FILE		*cur_file;
	char		objname[PATH_MAX];
	char		*cache;
	char		*sig;
	mrpc_status_t	retval;
	int		cachemiss;
	int		err;

	dev = (sdevice_state_t *) handle;

	sx.sig_val_x_val = (char *)obj_sig;
	sx.sig_val_x_len = sizeof(sig_val_t);

	retval = rpc_client_content_device_set_obj
					(dev->con_data.rpc_client, &sx);
	/* cache misses are not really errors, we don't need them logged */
	cachemiss = (retval == DIAMOND_FCACHEMISS);
	if (cachemiss) retval = DIAMOND_SUCCESS;

	err = rpc_postproc(__FUNCTION__, retval);
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
		goto err_out;
	}
	buf_len = stats.st_size;

	err = -1;
	data = (char *)malloc(buf_len);
	if(data == NULL) {
	  perror("malloc");
	  log_message(LOGT_NET, LOGL_ERR,
		      "device_set_lib: failed malloc spec file <%s>", objname);
	  goto err_out;
	}

	/*
	 * set data to the beginning of the data portion  and
	 * copy in the filter spec from the file.  NOTE: This is
	 * currently blocks, we may want to do something else later.
	 */
	if ((cur_file = fopen(objname, "r")) == NULL) {
		log_message(LOGT_NET, LOGL_ERR,
			    "device_set_lib: failed open <%s>", objname);
		goto err_out;
	}
	if ((rsize = fread(data, buf_len, 1, cur_file)) != 1) {
		log_message(LOGT_NET, LOGL_ERR,
		    "device_set_lib: failed read obj <%s>", objname);
		goto err_out;
	}

	fclose(cur_file);

	ox.obj_sig.sig_val_x_val = (char *)obj_sig;
	ox.obj_sig.sig_val_x_len = sizeof(sig_val_t);

	ox.obj_data.obj_data_len = buf_len;
	ox.obj_data.obj_data_val = data;

	retval = rpc_client_content_device_send_obj
					(dev->con_data.rpc_client, &ox);
	err = rpc_postproc(__FUNCTION__, retval);
err_out:
	if (data) free(data);
	return err;
}



int
device_write_leaf(void *handle, char *path, int len, char *data)
{
	dctl_x		*drx = NULL;
	char		*buf = NULL;
	sdevice_state_t *dev;
	dctl_x		dx;
	int		plen;
	mrpc_status_t	retval;
	int		err;

	dev = (sdevice_state_t *) handle;

	plen = strlen(path) + 1;

	buf = malloc(plen+len);
	if(buf == NULL) {
		log_message(LOGT_NET, LOGL_ERR,
			    "device_write_leaf: failed malloc data");
		return -1;
	}

	memcpy(&buf[0], path, plen);
	memcpy(&buf[plen], data, len);

	dx.dctl_err = 0;
	dx.dctl_plen = plen;
	dx.dctl_data.dctl_data_len = plen+len;
	dx.dctl_data.dctl_data_val = buf;

	retval = rpc_client_content_device_write_leaf
				(dev->con_data.rpc_client, &dx, &drx);

	err = rpc_postproc(__FUNCTION__, retval);
	if (err) goto err_out;

	err = drx->dctl_err;

err_out:
	if (buf) free(buf);
	free_dctl_x(drx, 1);
	return err;
}

int
device_read_leaf(void *handle, char *path,
		 dctl_data_type_t *dtype, int *dlen, void *dval)
{
	dctl_x		*drx = NULL;
	sdevice_state_t *dev;
	dctl_x		dx;
	int		len;
	mrpc_status_t	retval;
	int		err;

	dev = (sdevice_state_t *) handle;

	len = strlen(path) + 1;

	dx.dctl_err = 0;
	dx.dctl_plen = len;
	dx.dctl_data.dctl_data_len = len;
	dx.dctl_data.dctl_data_val = path;

	retval = rpc_client_content_device_read_leaf
				(dev->con_data.rpc_client, &dx, &drx);
	err = rpc_postproc(__FUNCTION__, retval);
	if (err) goto err_out;

	err = drx->dctl_err;
	len = drx->dctl_data.dctl_data_len;
	if (len <= *dlen)
		memcpy(dval, drx->dctl_data.dctl_data_val, len);
	else
		err = ENOMEM;
	*dlen = len;
	*dtype = drx->dctl_dtype;

err_out:
	free_dctl_x(drx, 1);
	return err;
}


int
device_list_nodes(void *handle, char *path, int *dents, dctl_entry_t *dval)
{
	dctl_x		*drx = NULL;
	sdevice_state_t *dev;
	int		len, ents;
	dctl_x		dx;
	mrpc_status_t	retval;
	int		err;

	dev = (sdevice_state_t *) handle;

	len = strlen(path) + 1;

	dx.dctl_err = 0;
	dx.dctl_plen = len;
	dx.dctl_data.dctl_data_len = len;
	dx.dctl_data.dctl_data_val = path;

	retval = rpc_client_content_device_list_nodes
				(dev->con_data.rpc_client, &dx, &drx);
	err = rpc_postproc(__FUNCTION__, retval);
	if (err) goto err_out;

	err = drx->dctl_err;
	ents = drx->dctl_data.dctl_data_len / sizeof(*dval);
	if (ents <= *dents)
		memcpy(dval, drx->dctl_data.dctl_data_val, ents*sizeof(*dval));
	else
		err = ENOMEM;
	*dents = ents;

err_out:
	free_dctl_x(drx, 1);
	return err;
}

int
device_list_leafs(void *handle, char *path, int *dents, dctl_entry_t *dval)
{
	dctl_x		*drx = NULL;
	sdevice_state_t *dev;
	int		len, ents;
	dctl_x		dx;
	mrpc_status_t	retval;
	int		err;

	dev = (sdevice_state_t *) handle;

	len = strlen(path) + 1;

	dx.dctl_err = 0;
	dx.dctl_plen = len;
	dx.dctl_data.dctl_data_len = len;
	dx.dctl_data.dctl_data_val = path;

	retval = rpc_client_content_device_list_leafs
				(dev->con_data.rpc_client, &dx, &drx);
	err = rpc_postproc(__FUNCTION__, retval);
	if (err) goto err_out;

	err = drx->dctl_err;
	ents = drx->dctl_data.dctl_data_len / sizeof(*dval);
	if (ents <= *dents)
		memcpy(dval, drx->dctl_data.dctl_data_val, ents*sizeof(*dval));
	else
		err = ENOMEM;
	*dents = ents;

err_out:
	free_dctl_x(drx, 1);
	return err;
}

int
device_set_blob(void *handle, char *name, int blob_len, void *blob)
{
	sdevice_state_t *dev;
	blob_x		bx;
	mrpc_status_t	retval;

	dev = (sdevice_state_t *) handle;

	bx.filter_name = name;
	bx.blob_data.blob_data_len = blob_len;
	bx.blob_data.blob_data_val = blob;

	retval = rpc_client_content_device_set_blob
					(dev->con_data.rpc_client, &bx);
	return rpc_postproc(__FUNCTION__, retval);
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
device_set_exec_mode(void *handle, uint32_t mode)
{
	sdevice_state_t *dev;
	mrpc_status_t	retval;

	dev = (sdevice_state_t *) handle;

	retval = rpc_client_content_device_set_exec_mode
					(dev->con_data.rpc_client, &mode);

	return rpc_postproc(__FUNCTION__, retval);
}


int
device_set_user_state(void *handle, uint32_t state)
{
	sdevice_state_t	*dev;
	mrpc_status_t	retval;

	dev = (sdevice_state_t *) handle;

	retval = rpc_client_content_device_set_user_state
					(dev->con_data.rpc_client, &state);

	return rpc_postproc(__FUNCTION__, retval);
}

int
device_get_session_variables(void *handle, device_session_vars_t **vars)
{
	sdevice_state_t *dev = (sdevice_state_t *) handle;
	diamond_session_vars_x *rc = NULL;
	mrpc_status_t	retval;
	int		i, err;

	*vars = NULL;

	retval = rpc_client_content_session_variables_get
					(dev->con_data.rpc_client, &rc);
	err = rpc_postproc(__FUNCTION__, retval);
	if (err) goto err_out;

	// allocate
	*vars = calloc(1, sizeof(device_session_vars_t));
	if (*vars == NULL) {
		log_message(LOGT_NET, LOGL_ERR,
			    "device_get_session_variables: no memory");
		err = -1;
		goto err_out;
	}

	(*vars)->len = rc->vars.vars_len;
	(*vars)->names = calloc((*vars)->len, sizeof(char *));
	(*vars)->values = calloc((*vars)->len, sizeof(double));

	for (i = 0; i < (*vars)->len; i++) {
		(*vars)->names[i] = strdup(rc->vars.vars_val[i].name);
		(*vars)->values[i] = rc->vars.vars_val[i].value;
	}
err_out:
	free_diamond_session_vars_x(rc, 1);
	return err;
}

int
device_set_session_variables(void *handle, device_session_vars_t *vars)
{
	sdevice_state_t	*dev = (sdevice_state_t *) handle;
	diamond_session_vars_x out;
	mrpc_status_t	retval;
	int		i;

	if (vars->len == 0)
		return 0;

	out.vars.vars_val = calloc(vars->len, sizeof(diamond_session_var_x));
	if (out.vars.vars_val == NULL)
		return -1;
	out.vars.vars_len = vars->len;

	for (i = 0; i < vars->len; i++) {
		out.vars.vars_val[i].name = vars->names[i];
		out.vars.vars_val[i].value = vars->values[i];
	}

	retval = rpc_client_content_session_variables_set
					(dev->con_data.rpc_client, &out);
	free(out.vars.vars_val);

	return rpc_postproc(__FUNCTION__, retval);
}


static void
setup_stats(sdevice_state_t * dev, const char *host)
{
	int		err;
	char		*node_name, *delim;
	char		path_name[128];	/* XXX */

	node_name = strdup(host);

	/*
	 * replace all the '.' with '_'
	 */
	while ((delim = index(node_name, '.')) != NULL)
		*delim = '_';

	snprintf(path_name, 128, "%s.%s", HOST_NETWORK_PATH, node_name);

	err = dctl_register_node(HOST_NETWORK_PATH, node_name);
	assert(err == 0);

	dctl_register_u32(path_name, "obj_rx", O_RDONLY,
			  &dev->con_data.stat_obj_rx);
	dctl_register_u64(path_name, "obj_total_bytes_rx", O_RDONLY,
			  &dev->con_data.stat_obj_total_byte_rx);
	dctl_register_u64(path_name, "obj_hdr_bytes_rx", O_RDONLY,
			  &dev->con_data.stat_obj_hdr_byte_rx);
	dctl_register_u64(path_name, "obj_attr_bytes_rx", O_RDONLY,
			  &dev->con_data.stat_obj_attr_byte_rx);
	dctl_register_u64(path_name, "obj_data_bytes_rx", O_RDONLY,
			  &dev->con_data.stat_obj_data_byte_rx);

	dctl_register_u32(path_name, "control_rx", O_RDONLY,
			  &dev->con_data.stat_control_rx);
	dctl_register_u64(path_name, "control_byte_rx", O_RDONLY,
			  &dev->con_data.stat_control_byte_rx);
	dctl_register_u32(path_name, "control_tx", O_RDONLY,
			  &dev->con_data.stat_control_tx);
	dctl_register_u64(path_name, "control_byte_tx", O_RDONLY,
			  &dev->con_data.stat_control_byte_tx);
	dctl_register_u32(path_name, "log_rx", O_RDONLY,
			  &dev->con_data.stat_log_rx);
	dctl_register_u64(path_name, "log_byte_rx", O_RDONLY,
			  &dev->con_data.stat_log_byte_rx);
	free(node_name);
}

/*
 * This is the initialization function that is
 * called by the searchlet library when we startup.
 */
void *
device_init(const char *host, void *hcookie, hstub_cb_args_t *cb_list)
{
	sdevice_state_t *new_dev;
	int		err;

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
	err = hstub_establish_connection(new_dev, host);
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

	err = pthread_create(&new_dev->thread_id, NULL, hstub_main, new_dev);
	if (err) {
		log_message(LOGT_NET, LOGL_ERR,
			    "device_init: failed to create thread");
		free(new_dev);
		return (NULL);
	}
	return ((void *) new_dev);
}
