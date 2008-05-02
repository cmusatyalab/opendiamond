/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 3
 *
 *  Copyright (c) 2002-2007 Intel Corporation
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
 *  Copyright (c) 2007-2008 Carnegie Mellon University
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
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
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
#include "lib_dconfig.h"
#include "sstub_impl.h"

#include <minirpc/minirpc.h>
#include "rpc_client_content_server.h"

static mrpc_status_t
device_start(void *conn_data, struct mrpc_message *msg)
{
	cstate_t *cstate = (cstate_t *)conn_data;

	fprintf(stderr, "have_start pend %d\n", cstate->pend_obj);
	if (cstate->pend_obj == 0) {
		(*cstate->lstate->cb.start_cb) (cstate->app_cookie);
	} else {
		cstate->have_start = 1;
	}
	return MINIRPC_OK;
}


static mrpc_status_t
device_stop(void *conn_data, struct mrpc_message *msg, stop_x *in)
{
	cstate_t *cstate = (cstate_t *)conn_data;
	host_stats_t hstats;

	hstats.hs_objs_received = in->host_objs_received;
	hstats.hs_objs_queued = in->host_objs_queued;
	hstats.hs_objs_read = in->host_objs_read;
	hstats.hs_objs_uqueued = in->app_objs_queued;
	hstats.hs_objs_upresented = in->app_objs_presented;

	(*cstate->lstate->cb.stop_cb) (cstate->app_cookie, &hstats);
	return MINIRPC_OK;
}


static mrpc_status_t
device_terminate(void *conn_data, struct mrpc_message *msg)
{
	cstate_t *cstate = (cstate_t *)conn_data;
	(*cstate->lstate->cb.terminate_cb) (cstate->app_cookie);
	return MINIRPC_OK;
}


static mrpc_status_t
device_clear_gids(void *conn_data, struct mrpc_message *msg)
{
	cstate_t *cstate = (cstate_t *)conn_data;
	(*cstate->lstate->cb.clear_gids_cb) (cstate->app_cookie);
	return MINIRPC_OK;
}


static mrpc_status_t
device_new_gid(void *conn_data, struct mrpc_message *msg, groupid_x *in)
{
	cstate_t *cstate = (cstate_t *)conn_data;
	groupid_t gid = *in;
	(*cstate->lstate->cb.sgid_cb) (cstate->app_cookie, gid);
	return MINIRPC_OK;
}


static mrpc_status_t
device_set_spec(void *conn_data, struct mrpc_message *msg, spec_file_x *in)
{
	cstate_t *cstate = (cstate_t *)conn_data;
	char specpath[PATH_MAX];
	char *cache;
	char *spec_sig, *spec_data;
	sig_val_t *sent_sig;
	int spec_len;
	int fd;

	spec_len = in->data.data_len;
	sent_sig = (sig_val_t *)&in->sig.sig_val_x_val;

	/*
	 * create a file for storing the searchlet library.
	 */
	umask(0000);

	cache = dconf_get_spec_cachedir();
	spec_sig = sig_string(sent_sig);
	snprintf(specpath, PATH_MAX, SPEC_FORMAT, cache, spec_sig);
	free(spec_sig);
	free(cache);

	spec_data = in->data.data_val;

	/* create the new file */
	file_get_lock(specpath);
	fd = open(specpath, O_CREAT | O_EXCL | O_WRONLY, 0744);
	if (fd < 0) {
		int err = errno;
		file_release_lock(specpath);
		if (err != EEXIST)
			return err;
		goto done;
	}
	if (write(fd, spec_data, spec_len) != spec_len) {
		perror("write buffer file");
		return errno;
	}
	close(fd);
	file_release_lock(specpath);

done:
	(*cstate->lstate->cb.set_fspec_cb) (cstate->app_cookie, sent_sig);
	return MINIRPC_OK;
}


static mrpc_status_t
device_set_blob(void *conn_data, struct mrpc_message *msg, blob_x *in)
{
	cstate_t *cstate = (cstate_t *)conn_data;
	char	*name;
	int	blen;
	void	*blob;

	name = in->filter_name;
	blen = in->blob_data.blob_data_len;
	blob = in->blob_data.blob_data_val;

	(*cstate->lstate->cb.set_blob_cb)(cstate->app_cookie, name, blen, blob);
	return MINIRPC_OK;
}


static mrpc_status_t
request_stats(void *conn_data, struct mrpc_message *msg, dev_stats_x *out)
{
	cstate_t *cstate = (cstate_t *)conn_data;
	dev_stats_t *stats;
	filter_stats_t *in_fstats;
	filter_stats_x *out_fstats;
	int i;

	stats = (*cstate->lstate->cb.get_stats_cb) (cstate->app_cookie);
	if (stats == NULL)
		return DIAMOND_NOSTATSAVAIL;

	out->ds_objs_total = stats->ds_objs_total;
	out->ds_objs_processed = stats->ds_objs_processed;
	out->ds_objs_dropped = stats->ds_objs_dropped;
	out->ds_objs_nproc = stats->ds_objs_nproc;
	out->ds_system_load = stats->ds_system_load;
	out->ds_avg_obj_time = stats->ds_avg_obj_time;
	out->ds_filter_stats.ds_filter_stats_len = stats->ds_num_filters;

	out->ds_filter_stats.ds_filter_stats_val = (filter_stats_x *)
		malloc(stats->ds_num_filters * sizeof(filter_stats_x));
	if (out->ds_filter_stats.ds_filter_stats_val == NULL) {
		perror("malloc");
		return DIAMOND_NOMEM;
	}

	for(i = 0; i < stats->ds_num_filters; i++) {
		in_fstats = &stats->ds_filter_stats[i];
		out_fstats = &out->ds_filter_stats.ds_filter_stats_val[i];

		out_fstats->fs_name = strdup(in_fstats->fs_name);
		if (out_fstats->fs_name == NULL) {
			perror("malloc");
			return DIAMOND_NOMEM;
		}
		out_fstats->fs_objs_processed = in_fstats->fs_objs_processed;
		out_fstats->fs_objs_dropped = in_fstats->fs_objs_dropped;
		out_fstats->fs_objs_cache_dropped = in_fstats->fs_objs_cache_dropped;
		out_fstats->fs_objs_cache_passed = in_fstats->fs_objs_cache_passed;
		out_fstats->fs_objs_compute = in_fstats->fs_objs_compute;
		out_fstats->fs_hits_inter_session = in_fstats->fs_hits_inter_session;
		out_fstats->fs_hits_inter_query = in_fstats->fs_hits_inter_query;
		out_fstats->fs_hits_intra_query = in_fstats->fs_hits_intra_query;
		out_fstats->fs_avg_exec_time = in_fstats->fs_avg_exec_time;
	}
	free(stats);
	return MINIRPC_OK;
}


static mrpc_status_t
request_chars(void *conn_data, struct mrpc_message *msg, dev_char_x *out)
{
	cstate_t *cstate = (cstate_t *)conn_data;
	device_char_t *chars;

	chars = (*cstate->lstate->cb.get_char_cb) (cstate->app_cookie);
	if (chars == NULL)
		return DIAMOND_FAILURE; // XXX: be more specific?

	out->dcs_isa = chars->dc_isa;
	out->dcs_speed = chars->dc_speed;
	out->dcs_mem = chars->dc_mem;

	free(chars);
	return MINIRPC_OK;
}

static mrpc_status_t
device_write_leaf(void *conn_data, struct mrpc_message *msg,
		  dctl_x *in, dctl_x *out)
{
	cstate_t *cstate = (cstate_t *)conn_data;
	int err;

	err = (*cstate->lstate->cb.wleaf_cb)
			(cstate->app_cookie, in->dctl_data.dctl_data_val,
			 (in->dctl_data.dctl_data_len - in->dctl_plen),
			 &(in->dctl_data.dctl_data_val[in->dctl_plen]));

	out->dctl_err = err;

	return MINIRPC_OK;
}


static mrpc_status_t
device_read_leaf(void *conn_data, struct mrpc_message *msg,
		 dctl_x *in, dctl_x *out)
{
	cstate_t *cstate = (cstate_t *)conn_data;
	dctl_rleaf_t *rt;

	rt = (*cstate->lstate->cb.rleaf_cb) (cstate->app_cookie,
					     in->dctl_data.dctl_data_val);
	if (rt == NULL)
		return DIAMOND_FAILURE; // XXX: be more specific?

	out->dctl_dtype = rt->dt;
	out->dctl_data.dctl_data_len = rt->len;
	out->dctl_data.dctl_data_val = rt->dbuf;

	free(rt);
	return MINIRPC_OK;
}


static mrpc_status_t
device_list_nodes(void *conn_data, struct mrpc_message *msg,
		  dctl_x *in, dctl_x *out)
{
	cstate_t *cstate = (cstate_t *)conn_data;
	dctl_lnode_t *lt;

	lt = (*cstate->lstate->cb.lnode_cb) (cstate->app_cookie,
					     in->dctl_data.dctl_data_val);
	if (lt == NULL)
		return DIAMOND_FAILURE;

	out->dctl_err = lt->err;
	out->dctl_data.dctl_data_len = lt->num_ents * sizeof(dctl_entry_t);
	out->dctl_data.dctl_data_val = (char *)lt->ent_data;

	free(lt);
	return MINIRPC_OK;
}


static mrpc_status_t
device_list_leafs(void *conn_data, struct mrpc_message *msg,
		  dctl_x *in, dctl_x *out)
{
	cstate_t *cstate = (cstate_t *)conn_data;
	dctl_lleaf_t *lt;

	lt = (*cstate->lstate->cb.lleaf_cb) (cstate->app_cookie,
					     in->dctl_data.dctl_data_val);
	out->dctl_err = lt->err;
	out->dctl_data.dctl_data_len = lt->num_ents * sizeof(dctl_entry_t);
	out->dctl_data.dctl_data_val = (char *)lt->ent_data;

	free(lt);
	return MINIRPC_OK;
}


static mrpc_status_t
device_set_exec_mode(void *conn_data, struct mrpc_message *msg, mode_x *in)
{
	cstate_t *cstate = (cstate_t *)conn_data;
	unsigned int mode = *in;
	(*cstate->lstate->cb.set_exec_mode_cb) (cstate->app_cookie, mode);
	return MINIRPC_OK;
}


static mrpc_status_t
device_set_user_state(void *conn_data, struct mrpc_message *msg, state_x *in)
{
	cstate_t *cstate = (cstate_t *)conn_data;
	unsigned int state = *in;
	(*cstate->lstate->cb.set_user_state_cb) (cstate->app_cookie, state);
	return MINIRPC_OK;
}

static mrpc_status_t
device_set_obj(void *conn_data, struct mrpc_message *msg, sig_val_x *in)
{
	cstate_t *cstate = (cstate_t *)conn_data;
	char objpath[PATH_MAX];
	char * cache;
	char * sig_str;
	sig_val_t *sent_sig;
	int err;

	if (in->sig_val_x_len != sizeof(sig_val_t))
		return DIAMOND_FAILURE;

	sent_sig = (sig_val_t *)(in->sig_val_x_val);

	/*
	 * create a file for storing the searchlet library.
	 */
	umask(0000);

	cache = dconf_get_binary_cachedir();
	sig_str = sig_string(sent_sig);
	snprintf(objpath, PATH_MAX, SO_FORMAT, cache, sig_str);
	free(sig_str);
	free(cache);

	if (access(objpath, F_OK) != 0) {
		cstate->pend_obj++;
		return DIAMOND_FCACHEMISS;
	}

	err = (*cstate->lstate->cb.set_fobj_cb) (cstate->app_cookie, sent_sig);
	if (err)
		return DIAMOND_FAILURE; // XXX: be more specific

	return MINIRPC_OK;
}


static mrpc_status_t
device_send_obj(void *conn_data, struct mrpc_message *msg, obj_x *in)
{
	cstate_t *cstate = (cstate_t *)conn_data;
	int fd;
	char objname[PATH_MAX];
	sig_val_t calc_sig;
	sig_val_t *sent_sig;
	char * sig_str;
	char *cache;
	int err;
	ssize_t n;

	sent_sig = (sig_val_t *)(in->obj_sig.sig_val_x_val);

	/* get name to store the object */
	cache = dconf_get_binary_cachedir();
	sig_str = sig_string(sent_sig);
	snprintf(objname, PATH_MAX, SO_FORMAT, cache, sig_str);
	free(sig_str);
	free(cache);

	/* check whether the calculated signature matches the sent one */
	sig_cal(in->obj_data.obj_data_val, in->obj_data.obj_data_len,&calc_sig);
	if (memcmp(&calc_sig, sent_sig, sizeof(sig_val_t)) != 0) {
		fprintf(stderr, "data doesn't match sig\n");
		return EINVAL;
	}

	/* create the new file */
	file_get_lock(objname);
	fd = open(objname, O_CREAT | O_EXCL | O_WRONLY, 0744);
	if (fd < 0) {
		file_release_lock(objname);
		if (errno != EEXIST)
			return errno;

		return MINIRPC_OK;
	}
	n = writen(fd, in->obj_data.obj_data_val, in->obj_data.obj_data_len);
	if (n != (ssize_t)in->obj_data.obj_data_len) {
		perror("write buffer file");
		err = errno;
		close(fd);
		return err;
	}
	close(fd);
	file_release_lock(objname);

	err = (*cstate->lstate->cb.set_fobj_cb) (cstate->app_cookie, sent_sig);
	if(err)
		return DIAMOND_FAILURE; // XXX: be more specific

	cstate->pend_obj--;

	if (cstate->pend_obj== 0 && cstate->have_start) {
		(*cstate->lstate->cb.start_cb) (cstate->app_cookie);
		cstate->have_start = 0;
	}
	return MINIRPC_OK;
}


/* for anomaly detection */
static mrpc_status_t
session_variables_get(void *conn_data, struct mrpc_message *msg,
		      diamond_session_vars_x *out)
{
	cstate_t *cstate = (cstate_t *)conn_data;
	device_session_vars_t *vars;
	int i, err = DIAMOND_NOMEM;

	vars = (*cstate->lstate->cb.get_session_vars_cb) (cstate->app_cookie);
	if (vars == NULL) goto err_out;

	out->vars.vars_val = calloc(vars->len, sizeof(diamond_session_var_x));
	if (out->vars.vars_val == NULL) {
		for (i = 0; i < vars->len; i++)
			free(vars->names[i]);
		goto err_out;
	}
	out->vars.vars_len = vars->len;

	for (i = 0; i < vars->len; i++) {
		out->vars.vars_val[i].name = vars->names[i];
		out->vars.vars_val[i].value = vars->values[i];
	}
	err = MINIRPC_OK;
err_out:
	free(vars->names);
	free(vars->values);
	free(vars);
	return err;
}

static mrpc_status_t
session_variables_set(void *conn_data, struct mrpc_message *msg,
		      diamond_session_vars_x *in)
{
	cstate_t *cstate = (cstate_t *)conn_data;
	device_session_vars_t *vars;
	int i, err = DIAMOND_NOMEM;

	vars = calloc(1, sizeof(device_session_vars_t));
	if (vars == NULL)
		goto err_out;

	vars->len = in->vars.vars_len;
	vars->names = calloc(vars->len, sizeof(char *));
	vars->values = calloc(vars->len, sizeof(double));

	if (vars->names == NULL || vars->values == NULL)
		goto err_out;

	for (i = 0; i < vars->len; i++) {
		vars->names[i] = strdup(in->vars.vars_val[i].name);
		vars->values[i] = in->vars.vars_val[i].value;
	}

	(*cstate->lstate->cb.set_session_vars_cb) (cstate->app_cookie, vars);
	err = MINIRPC_OK;
err_out:
	free(vars->names);
	free(vars->values);
	free(vars);
	return err;
}

static const struct rpc_client_content_server_operations ops = {
	.device_start = device_start,
	.device_stop = device_stop,
	.device_terminate = device_terminate,
	.device_clear_gids = device_clear_gids,
	.device_new_gid = device_new_gid,
	.device_set_spec = device_set_spec,
	.device_write_leaf = device_write_leaf,
	.device_read_leaf = device_read_leaf,
	.device_list_nodes = device_list_nodes,
	.device_list_leafs = device_list_leafs,
	.device_set_blob = device_set_blob,
	.device_set_exec_mode = device_set_exec_mode,
	.device_set_user_state = device_set_user_state,
	.request_chars = request_chars,
	.request_stats = request_stats,
	.device_set_obj = device_set_obj,
	.device_send_obj = device_send_obj,
	.session_variables_get = session_variables_get,
	.session_variables_set = session_variables_set,
};
const struct rpc_client_content_server_operations *sstub_ops = &ops;
