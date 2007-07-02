/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 2
 *
 *  Copyright (c) 2002-2007 Intel Corporation
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
 *  Copyright (c) 2007 Carnegie Mellon University
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
#include "lib_auth.h"
#include "lib_sstub.h"
#include "lib_dconfig.h"
#include "sstub_impl.h"
#include "rpc_client_content.h"

static char const cvsid[] =
    "$Header$";


void
sstub_except_control(listener_state_t * lstate, cstate_t * cstate)
{
	printf("XXX except_control \n");
	/*
	 * Handle the case where we are shutting down 
	 */
	if (cstate->flags & CSTATE_SHUTTING_DOWN) {
		return;
	}

	return;
}


diamond_rc_t *
device_start_x_2_svc(u_int gen,  struct svc_req *rqstp)
{
	static diamond_rc_t  result;

	fprintf(stderr, "have_start pend %d \n", tirpc_cstate->pend_obj);
	if (tirpc_cstate->pend_obj == 0) {
	  (*tirpc_lstate->start_cb) (tirpc_cstate->app_cookie, gen);
	} else {
	  tirpc_cstate->have_start = 1;
	  tirpc_cstate->start_gen = gen;
	}

	result.service_err = DIAMOND_SUCCESS;
	return &result;
}


diamond_rc_t *
device_stop_x_2_svc(u_int gen, stop_x arg2,  struct svc_req *rqstp)
{
	static diamond_rc_t  result;
	host_stats_t hstats;

	hstats.hs_objs_received = arg2.host_objs_received;
	hstats.hs_objs_queued = arg2.host_objs_queued;
	hstats.hs_objs_read = arg2.host_objs_read;
	hstats.hs_objs_uqueued = arg2.app_objs_queued;
	hstats.hs_objs_upresented = arg2.app_objs_presented;
	(*tirpc_lstate->stop_cb) (tirpc_cstate->app_cookie, gen, &hstats);

	result.service_err = DIAMOND_SUCCESS;
	return &result;
}


diamond_rc_t *
device_terminate_x_2_svc(u_int gen,  struct svc_req *rqstp)
{
	static diamond_rc_t  result;

	(*tirpc_lstate->terminate_cb) (tirpc_cstate->app_cookie, gen);

	result.service_err = DIAMOND_SUCCESS;
	return &result;
}


diamond_rc_t *
device_clear_gids_x_2_svc(u_int gen,  struct svc_req *rqstp)
{
	static diamond_rc_t  result;

	(*tirpc_lstate->clear_gids_cb) (tirpc_cstate->app_cookie, gen);				
	result.service_err = DIAMOND_SUCCESS;
	return &result;
}


diamond_rc_t *
device_new_gid_x_2_svc(u_int gen, groupid_x arg2,  struct svc_req *rqstp)
{
	static diamond_rc_t  result;
	groupid_t       gid = arg2;
	
	(*tirpc_lstate->sgid_cb) (tirpc_cstate->app_cookie, gen, gid);

	result.service_err = DIAMOND_SUCCESS;
	return &result;
}


diamond_rc_t *
device_set_blob_x_2_svc(u_int gen, blob_x arg2, struct svc_req *rqstp)
{
	static diamond_rc_t  result;
	void                *blob;
	int                  blen;
	int                  nlen;
	char                *name;
	
	nlen = arg2.blob_name.blob_name_len;
	blen = arg2.blob_data.blob_data_len;
	name = arg2.blob_name.blob_name_val;
	blob = arg2.blob_data.blob_data_val;
	
	(*tirpc_lstate->set_blob_cb) (tirpc_cstate->app_cookie, gen, 
				      name, blen, blob);

	result.service_err = DIAMOND_SUCCESS;
	return &result;
}


diamond_rc_t *
device_set_spec_x_2_svc(u_int gen, spec_file_x arg2,  struct svc_req *rqstp)
{
	static diamond_rc_t  result;
	char specpath[PATH_MAX];
	char * cache;
	char *spec_sig, *spec_data;
	sig_val_t sent_sig;
	int spec_len;
	int fd;

	spec_len = arg2.data.data_len;
	memcpy(&sent_sig, &arg2.sig, sizeof(sig_val_t));  /* sig_val_x and
							   * sig_val_t should
							   * be identical
							   * in size. */

	/*
	 * create a file for storing the searchlet library.
	 */
	umask(0000);

	cache = dconf_get_spec_cachedir();
	spec_sig = sig_string(&sent_sig);
	snprintf(specpath, PATH_MAX, SPEC_FORMAT, cache, spec_sig);
	free(spec_sig);
	free(cache);

	spec_data = arg2.data.data_val;

        /* create the new file */
	file_get_lock(specpath);
	fd = open(specpath, O_CREAT|O_EXCL|O_WRONLY, 0744);
       	if (fd < 0) {
	        int err = errno;
		file_release_lock(specpath);
		if (err == EEXIST) { 
			goto done; 
		}
		fprintf(stderr, "file %s failed on %d \n", specpath, err); 
		result.service_err = DIAMOND_FAILEDSYSCALL;
		result.opcode_err = err;
		return &result;
	}
	if (write(fd, spec_data, spec_len) != spec_len) {
		perror("write buffer file"); 
		result.service_err = DIAMOND_FAILEDSYSCALL;
		result.opcode_err = errno;
		return &result;
	}
	close(fd);
	file_release_lock(specpath);

done:
	(*tirpc_lstate->set_fspec_cb)(tirpc_cstate->app_cookie, gen, 
				      &sent_sig);

	result.service_err = DIAMOND_SUCCESS;
	return &result;
}

request_stats_return_x *
request_stats_x_2_svc(u_int gen, struct svc_req *rqstp)
{
  static request_stats_return_x  result;
  dev_stats_t *stats;
  int i;

  if((stats = (*tirpc_lstate->get_stats_cb) (tirpc_cstate->app_cookie, gen)) == NULL) {
    result.error.service_err = DIAMOND_OPERR;
    result.error.opcode_err = DIAMOND_OPCODE_FAILURE; //XXX: be more specific?
    return &result;
  }
  
  result.stats.ds_objs_total = stats->ds_objs_total;
  result.stats.ds_objs_processed = stats->ds_objs_processed;
  result.stats.ds_objs_dropped = stats->ds_objs_dropped;
  result.stats.ds_objs_nproc = stats->ds_objs_nproc;
  result.stats.ds_system_load = stats->ds_system_load;
  result.stats.ds_avg_obj_time = stats->ds_avg_obj_time;
  result.stats.ds_filter_stats.ds_filter_stats_len = stats->ds_num_filters;
  
  if((result.stats.ds_filter_stats.ds_filter_stats_val = (filter_stats_x *)malloc(stats->ds_num_filters * sizeof(filter_stats_x))) == NULL) {
    perror("malloc");
    result.error.service_err = DIAMOND_NOMEM;
    return &result;
  }
  
  for(i=0; i<stats->ds_num_filters; i++) {
    if((result.stats.ds_filter_stats.ds_filter_stats_val[i].fs_name = strdup(stats->ds_filter_stats[i].fs_name)) == NULL) {
      perror("malloc"); 
      result.error.service_err = DIAMOND_NOMEM; 
      return &result; 
    } 
    result.stats.ds_filter_stats.ds_filter_stats_val[i].fs_objs_processed = stats->ds_filter_stats[i].fs_objs_processed;
    result.stats.ds_filter_stats.ds_filter_stats_val[i].fs_objs_dropped = stats->ds_filter_stats[i].fs_objs_dropped;
    result.stats.ds_filter_stats.ds_filter_stats_val[i].fs_objs_cache_dropped = stats->ds_filter_stats[i].fs_objs_cache_dropped;
    result.stats.ds_filter_stats.ds_filter_stats_val[i].fs_objs_cache_passed = stats->ds_filter_stats[i].fs_objs_cache_passed;
    result.stats.ds_filter_stats.ds_filter_stats_val[i].fs_objs_compute = stats->ds_filter_stats[i].fs_objs_compute;
    result.stats.ds_filter_stats.ds_filter_stats_val[i].fs_hits_inter_session = stats->ds_filter_stats[i].fs_hits_inter_session;
    result.stats.ds_filter_stats.ds_filter_stats_val[i].fs_hits_inter_query = stats->ds_filter_stats[i].fs_hits_inter_query;
    result.stats.ds_filter_stats.ds_filter_stats_val[i].fs_hits_intra_query = stats->ds_filter_stats[i].fs_hits_intra_query;
    result.stats.ds_filter_stats.ds_filter_stats_val[i].fs_avg_exec_time = stats->ds_filter_stats[i].fs_avg_exec_time;
  }

  free(stats);

  result.error.service_err = DIAMOND_SUCCESS;
  return &result;
}

request_chars_return_x *
request_chars_x_2_svc(u_int gen,  struct svc_req *rqstp)
{
  static request_chars_return_x  result;
  device_char_t *chars;

  fprintf(stderr, "(TI-RPC server) Entering request characteristics call.\n");

  if((chars = (*tirpc_lstate->get_char_cb) (tirpc_cstate->app_cookie, gen)) == NULL) {
    result.error.service_err = DIAMOND_OPERR; 
    result.error.opcode_err = DIAMOND_OPCODE_FAILURE; //XXX: be more specific? 
    return &result; 
  }
  
  result.chars.dcs_isa = chars->dc_isa;
  result.chars.dcs_speed = chars->dc_speed;
  result.chars.dcs_mem = chars->dc_mem;

  free(chars);

  fprintf(stderr, "(TI-RPC server) Returning from request characteristics "
	  "call.\n");

  result.error.service_err = DIAMOND_SUCCESS;
  return &result;
}

dctl_return_x *
device_read_leaf_x_2_svc(u_int gen, dctl_x arg2,  struct svc_req *rqstp)
{
	static dctl_return_x  result;
	dctl_rleaf_t          *rt;
	
	rt = (tirpc_lstate->rleaf_cb) (tirpc_cstate->app_cookie,
				       arg2.dctl_data.dctl_data_val, 
				       arg2.dctl_opid);
	if(rt == NULL) {
	  result.error.service_err = DIAMOND_OPERR;
	  result.error.opcode_err = DIAMOND_FAILURE; //XXX: be more specific?
	  return &result;
	}
	
	result.dctl.dctl_err = 0;           //XXX: is this arg even necessary?
	result.dctl.dctl_opid = arg2.dctl_opid; //XXX: or this one?
	result.dctl.dctl_plen = 0;          //XXX: or this one?
	result.dctl.dctl_dtype = rt->dt;
	result.dctl.dctl_data.dctl_data_len = rt->len;
	result.dctl.dctl_data.dctl_data_val = rt->dbuf; /* TI-RPC will
							 * free dbuf */

	free(rt);

	result.error.service_err = DIAMOND_SUCCESS;
	return &result;
}


dctl_return_x *
device_write_leaf_x_2_svc(u_int gen, dctl_x arg2,  struct svc_req *rqstp)
{
	static dctl_return_x  result;
	int                   err;


	err = (*tirpc_lstate->wleaf_cb) (tirpc_cstate->app_cookie,
			      arg2.dctl_data.dctl_data_val,
					 (arg2.dctl_data.dctl_data_len -
					  arg2.dctl_plen),
			      &(arg2.dctl_data.dctl_data_val[arg2.dctl_plen]), 
			      arg2.dctl_opid);
	
	result.dctl.dctl_err = err;
	result.dctl.dctl_opid = arg2.dctl_opid;
	result.dctl.dctl_plen = 0;
	result.dctl.dctl_data.dctl_data_len = 0;
	result.dctl.dctl_data.dctl_data_val = NULL;

	if(err == 0)
	  result.error.service_err = DIAMOND_SUCCESS;
	else {
	  result.error.service_err = DIAMOND_OPERR;
	  result.error.opcode_err = DIAMOND_FAILURE;
	}

	return &result;
}


dctl_return_x *
device_list_nodes_x_2_svc(u_int gen, dctl_x arg2,  struct svc_req *rqstp)
{
	static dctl_return_x  result;
	dctl_lnode_t *lt;

	lt = (tirpc_lstate->lnode_cb) (tirpc_cstate->app_cookie,
					arg2.dctl_data.dctl_data_val, 
					arg2.dctl_opid);

	if(lt == NULL) {
	  result.error.service_err = DIAMOND_OPERR;
	  result.error.opcode_err = DIAMOND_FAILURE;
	  return &result;
	}

	result.dctl.dctl_err = lt->err;
	result.dctl.dctl_opid = arg2.dctl_opid;
	result.dctl.dctl_plen = 0;
	result.dctl.dctl_data.dctl_data_len = lt->num_ents * sizeof(dctl_entry_t);

	/* ent_data will be freed by TI-RPC. */
	result.dctl.dctl_data.dctl_data_val = (char *)lt->ent_data; 

	free(lt);

	result.error.service_err = DIAMOND_SUCCESS;
	return &result;
}


dctl_return_x *
device_list_leafs_x_2_svc(u_int gen, dctl_x arg2,  struct svc_req *rqstp)
{
	static dctl_return_x  result;
	dctl_lleaf_t *lt;

	lt = (tirpc_lstate->lleaf_cb) (tirpc_cstate->app_cookie,
				       arg2.dctl_data.dctl_data_val, 
				       arg2.dctl_opid);

	result.dctl.dctl_err = lt->err;
	result.dctl.dctl_opid = arg2.dctl_opid;
	result.dctl.dctl_plen = 0;
	result.dctl.dctl_data.dctl_data_len = lt->num_ents * sizeof(dctl_entry_t);

	/* ent_data will be freed by TI-RPC. */
	result.dctl.dctl_data.dctl_data_val = (char *)lt->ent_data;
	
	free(lt);

	result.error.service_err = DIAMOND_SUCCESS;
	return &result;
}


diamond_rc_t *
device_set_exec_mode_x_2_svc(u_int gen, u_int mode,  struct svc_req *rqstp)
{
	static diamond_rc_t  result;

	(tirpc_lstate->set_exec_mode_cb) (tirpc_cstate->app_cookie, mode);

	result.service_err = DIAMOND_SUCCESS;
	return &result;
}


diamond_rc_t *
device_set_user_state_x_2_svc(u_int gen, u_int state,  struct svc_req *rqstp)
{
	static diamond_rc_t  result;
	
	(tirpc_lstate->set_user_state_cb) (tirpc_cstate->app_cookie, state);

	result.service_err = DIAMOND_SUCCESS;
	return &result;
}

diamond_rc_t *
device_set_obj_x_2_svc(u_int gen, sig_val_x arg2,  struct svc_req *rqstp)
{
	static diamond_rc_t  result;
	char objpath[PATH_MAX];
	char * cache;
	char * sig_str;
	sig_val_t sent_sig;

	memcpy(&sent_sig, &arg2, sizeof(sig_val_t));

	/*
	 * create a file for storing the searchlet library.
	 */
	umask(0000);

	cache = dconf_get_binary_cachedir();
	sig_str = sig_string(&sent_sig);
	snprintf(objpath, PATH_MAX, OBJ_FORMAT, cache, sig_str);
	free(sig_str);
	free(cache);

	if (file_exists(objpath)) {
	  int err;
	  err = (*tirpc_lstate->set_fobj_cb) (tirpc_cstate->app_cookie, gen, 
					      &sent_sig);
	  if(err) {
	    result.service_err = DIAMOND_OPERR;
	    result.opcode_err = DIAMOND_OPCODE_FAILURE; //XXX: be more specific
	    return &result;
	  }

	} else {
	  tirpc_cstate->pend_obj++;
	  result.service_err = DIAMOND_OPERR;
	  result.opcode_err = DIAMOND_OPCODE_FCACHEMISS;
	  return &result;
	}

	result.service_err = DIAMOND_SUCCESS;
	return &result;
}


diamond_rc_t *
device_send_obj_x_2_svc(u_int gen, send_obj_x arg2,  struct svc_req *rqstp)
{
	static diamond_rc_t  result;
	int fd;
	char objname[PATH_MAX];
	sig_val_t calc_sig;
	sig_val_t *sent_sig;
	char * sig_str;
	char *cache;
	int err;

	sent_sig = (sig_val_t *)arg2.obj_sig.sig_val_x_val;

	/* get name to store the object */ 	
	cache = dconf_get_binary_cachedir();
	sig_str = sig_string(sent_sig);
	snprintf(objname, PATH_MAX, OBJ_FORMAT, cache, sig_str);
	free(sig_str);
	free(cache);

	/* check whether the calculated signature matches the sent one */
	sig_cal(arg2.obj_data.obj_data_val, arg2.obj_data.obj_data_len, 
		&calc_sig);
	if (memcmp(&calc_sig, sent_sig, sizeof(sig_val_t)) != 0) {
	  fprintf(stderr, "data doesn't match sig\n");
	}

        /* create the new file */
	file_get_lock(objname);
	fd = open(objname, O_CREAT|O_EXCL|O_WRONLY, 0744);
       	if (fd < 0) {
		file_release_lock(objname);
		if (errno == EEXIST) {
		  result.service_err = DIAMOND_SUCCESS;
		  return &result; 
		}
		fprintf(stderr, "file %s failed on %d \n", objname, errno); 
		result.service_err = DIAMOND_FAILEDSYSCALL;
		result.opcode_err = errno;
		return &result;
	} 
	if (write(fd, arg2.obj_data.obj_data_val, arg2.obj_data.obj_data_len) !=  arg2.obj_data.obj_data_len) {
		perror("write buffer file"); 
		result.service_err = DIAMOND_FAILEDSYSCALL;
		result.opcode_err = errno;
		close(fd);
		return &result;
	}
	close(fd);
	file_release_lock(objname);
	
	err = (*tirpc_lstate->set_fobj_cb) (tirpc_cstate->app_cookie, gen, 
					    sent_sig);
	if(err) {
	  result.service_err = DIAMOND_OPERR;
	  result.opcode_err = DIAMOND_OPCODE_FAILURE; //XXX: be more specific
	  return &result;
	}

	tirpc_cstate->pend_obj--;

	if((tirpc_cstate->pend_obj== 0) && (tirpc_cstate->have_start)) {
	  (*tirpc_lstate->start_cb) (tirpc_cstate->app_cookie, 
				     tirpc_cstate->start_gen);
	  tirpc_cstate->have_start = 0;
	}

	result.service_err = DIAMOND_SUCCESS;
	return &result;
}


/*
 * This reads data from the control socket and forwards it to the
 * TI-RPC server.
 */

void
sstub_read_control(listener_state_t * lstate, cstate_t * cstate)
{
	int size_in, size_out;
	char buf[4096];

	/*
	 * Handle the case where we are shutting down 
	 */
	if (cstate->flags & CSTATE_SHUTTING_DOWN) {
		printf("read control:  shutting down \n");
		return;
	}

	/* Attempt to process up to 4096 bytes of data. */
	
	size_in = read(cstate->control_fd, (void *)buf, 4096);
	if(size_in < 0) {
	  perror("read");
	  return;
	}
	else if(size_in == 0) { /* EOF */
	  close(cstate->control_fd);
	  fprintf(stderr, "(tunnel) Client closed control connection\n");
	  exit(EXIT_SUCCESS);	  
	}

	
	size_out = writen(cstate->tirpc_fd, (void *)buf, size_in);
	if(size_out < 0) {
	  perror("write");
	  return;
	}
	
	if(size_in != size_out) {
	  fprintf(stderr, "Somehow lost bytes, from %d in to %d out!\n", 
		  size_in, size_out);
	  return;
	}
	else printf("Forwarded %d control bytes.\n", size_in);

	return;
}
