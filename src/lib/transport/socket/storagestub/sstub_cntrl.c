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


static listener_state_t *rpc_lstate;
static cstate_t *rpc_cstate;


void
sstub_set_states(listener_state_t *new_lstate, cstate_t *new_cstate)
{
  rpc_lstate = new_lstate;
  rpc_cstate = new_cstate;
}

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

int
clientcontent_prog_2_freeresult (SVCXPRT *transp, xdrproc_t xdr_result, 
				 caddr_t result)
{
        xdr_free (xdr_result, result);

        return 1;
}

bool_t
device_start_x_2_svc(u_int gen, diamond_rc_t *result, struct svc_req *rqstp)
{
	memset ((char *)result, 0, sizeof(*result));

	fprintf(stderr, "have_start pend %d \n", rpc_cstate->pend_obj);
	if (rpc_cstate->pend_obj == 0) {
	  (*rpc_lstate->cb.start_cb) (rpc_cstate->app_cookie, gen);
	} else {
	  rpc_cstate->have_start = 1;
	  rpc_cstate->start_gen = gen;
	}

	result->service_err = DIAMOND_SUCCESS;
	return 1;
}


bool_t
device_stop_x_2_svc(u_int gen, stop_x arg2, diamond_rc_t *result,
		    struct svc_req *rqstp)
{
	host_stats_t hstats;

	memset ((char *)result, 0, sizeof(*result));

	hstats.hs_objs_received = arg2.host_objs_received;
	hstats.hs_objs_queued = arg2.host_objs_queued;
	hstats.hs_objs_read = arg2.host_objs_read;
	hstats.hs_objs_uqueued = arg2.app_objs_queued;
	hstats.hs_objs_upresented = arg2.app_objs_presented;

	(*rpc_lstate->cb.stop_cb) (rpc_cstate->app_cookie, gen, &hstats);

	result->service_err = DIAMOND_SUCCESS;
	return 1;
}


bool_t
device_terminate_x_2_svc(u_int gen, diamond_rc_t *result, 
			 struct svc_req *rqstp)
{
	memset ((char *)result, 0, sizeof(*result));

	(*rpc_lstate->cb.terminate_cb) (rpc_cstate->app_cookie, gen);

	result->service_err = DIAMOND_SUCCESS;
	return 1;
}


bool_t
device_clear_gids_x_2_svc(u_int gen, diamond_rc_t *result, 
			  struct svc_req *rqstp)
{
	memset ((char *)result, 0, sizeof(*result));

	(*rpc_lstate->cb.clear_gids_cb) (rpc_cstate->app_cookie, gen);
	result->service_err = DIAMOND_SUCCESS;
	return 1;
}


bool_t
device_new_gid_x_2_svc(u_int gen, groupid_x arg2, diamond_rc_t *result,
		       struct svc_req *rqstp)
{
	groupid_t       gid = arg2;

	memset ((char *)result, 0, sizeof(*result));

	(*rpc_lstate->cb.sgid_cb) (rpc_cstate->app_cookie, gen, gid);

	result->service_err = DIAMOND_SUCCESS;
	return 1;
}


bool_t
device_set_blob_x_2_svc(u_int gen, blob_x arg2, diamond_rc_t *result,
			struct svc_req *rqstp)
{
	void                *blob;
	int                  blen;
	int                  nlen;
	char                *name;

	memset ((char *)result, 0, sizeof(*result));
	
	nlen = arg2.blob_name.blob_name_len;
	blen = arg2.blob_data.blob_data_len;
	name = arg2.blob_name.blob_name_val;
	blob = arg2.blob_data.blob_data_val;
	
	(*rpc_lstate->cb.set_blob_cb) (rpc_cstate->app_cookie, gen,
				       name, blen, blob);

	result->service_err = DIAMOND_SUCCESS;
	return 1;
}


bool_t
device_set_spec_x_2_svc(u_int gen, spec_file_x arg2, diamond_rc_t *result,
			struct svc_req *rqstp)
{
	char specpath[PATH_MAX];
	char * cache;
	char *spec_sig, *spec_data;
	sig_val_t *sent_sig;
	int spec_len;
	int fd;

	memset ((char *)result, 0, sizeof(*result));

	spec_len = arg2.data.data_len;
	sent_sig = (sig_val_t *)&arg2.sig.sig_val_x_val;

	/*
	 * create a file for storing the searchlet library.
	 */
	umask(0000);

	cache = dconf_get_spec_cachedir();
	spec_sig = sig_string(sent_sig);
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
		result->service_err = DIAMOND_FAILEDSYSCALL;
		result->opcode_err = err;
		return 1;
	}
	if (write(fd, spec_data, spec_len) != spec_len) {
		perror("write buffer file"); 
		result->service_err = DIAMOND_FAILEDSYSCALL;
		result->opcode_err = errno;
		return 1;
	}
	close(fd);
	file_release_lock(specpath);

done:
	(*rpc_lstate->cb.set_fspec_cb) (rpc_cstate->app_cookie, gen, sent_sig);

	result->service_err = DIAMOND_SUCCESS;
	return 1;
}


bool_t
request_stats_x_2_svc(u_int gen, request_stats_return_x *result, 
		      struct svc_req *rqstp)
{
  dev_stats_t *stats;
  int i;

  memset ((char *)result, 0, sizeof(*result));

  stats = (*rpc_lstate->cb.get_stats_cb) (rpc_cstate->app_cookie, gen);
  if(stats == NULL) {
    result->error.service_err = DIAMOND_OPERR;
    result->error.opcode_err = DIAMOND_OPCODE_NOSTATSAVAIL;
    return 1;
  }

  result->stats.ds_objs_total = stats->ds_objs_total;
  result->stats.ds_objs_processed = stats->ds_objs_processed;
  result->stats.ds_objs_dropped = stats->ds_objs_dropped;
  result->stats.ds_objs_nproc = stats->ds_objs_nproc;
  result->stats.ds_system_load = stats->ds_system_load;
  result->stats.ds_avg_obj_time = stats->ds_avg_obj_time;
  result->stats.ds_filter_stats.ds_filter_stats_len = stats->ds_num_filters;
  
  if((result->stats.ds_filter_stats.ds_filter_stats_val = (filter_stats_x *)malloc(stats->ds_num_filters * sizeof(filter_stats_x))) == NULL) {
    perror("malloc");
    result->error.service_err = DIAMOND_NOMEM;
    return 1;
  }
  
  for(i=0; i<stats->ds_num_filters; i++) {
    if((result->stats.ds_filter_stats.ds_filter_stats_val[i].fs_name = strdup(stats->ds_filter_stats[i].fs_name)) == NULL) {
      perror("malloc"); 
      result->error.service_err = DIAMOND_NOMEM; 
      return 1;
    } 
    result->stats.ds_filter_stats.ds_filter_stats_val[i].fs_objs_processed = stats->ds_filter_stats[i].fs_objs_processed;
    result->stats.ds_filter_stats.ds_filter_stats_val[i].fs_objs_dropped = stats->ds_filter_stats[i].fs_objs_dropped;
    result->stats.ds_filter_stats.ds_filter_stats_val[i].fs_objs_cache_dropped = stats->ds_filter_stats[i].fs_objs_cache_dropped;
    result->stats.ds_filter_stats.ds_filter_stats_val[i].fs_objs_cache_passed = stats->ds_filter_stats[i].fs_objs_cache_passed;
    result->stats.ds_filter_stats.ds_filter_stats_val[i].fs_objs_compute = stats->ds_filter_stats[i].fs_objs_compute;
    result->stats.ds_filter_stats.ds_filter_stats_val[i].fs_hits_inter_session = stats->ds_filter_stats[i].fs_hits_inter_session;
    result->stats.ds_filter_stats.ds_filter_stats_val[i].fs_hits_inter_query = stats->ds_filter_stats[i].fs_hits_inter_query;
    result->stats.ds_filter_stats.ds_filter_stats_val[i].fs_hits_intra_query = stats->ds_filter_stats[i].fs_hits_intra_query;
    result->stats.ds_filter_stats.ds_filter_stats_val[i].fs_avg_exec_time = stats->ds_filter_stats[i].fs_avg_exec_time;
  }

  free(stats);

  result->error.service_err = DIAMOND_SUCCESS;
  return 1;
}


bool_t
request_chars_x_2_svc(u_int gen, request_chars_return_x *result, 
		      struct svc_req *rqstp)
{
  device_char_t *chars;

  memset ((char *)result, 0, sizeof(*result));

  chars = (*rpc_lstate->cb.get_char_cb) (rpc_cstate->app_cookie, gen);
  if(chars == NULL) {
    result->error.service_err = DIAMOND_OPERR; 
    result->error.opcode_err = DIAMOND_OPCODE_FAILURE;//XXX: be more specific? 
    return 1;
  }
  
  result->chars.dcs_isa = chars->dc_isa;
  result->chars.dcs_speed = chars->dc_speed;
  result->chars.dcs_mem = chars->dc_mem;

  free(chars);

  result->error.service_err = DIAMOND_SUCCESS;
  return 1;
}

bool_t
device_read_leaf_x_2_svc(u_int gen, dctl_x arg2, dctl_return_x *result,
			 struct svc_req *rqstp)
{
	dctl_rleaf_t          *rt;

	memset ((char *)result, 0, sizeof(*result));

	rt = (rpc_lstate->cb.rleaf_cb) (rpc_cstate->app_cookie,
					arg2.dctl_data.dctl_data_val);
	if(rt == NULL) {
	  result->error.service_err = DIAMOND_OPERR;
	  result->error.opcode_err = DIAMOND_FAILURE; //XXX: be more specific?
	  return 1;
	}
	
	result->dctl.dctl_err = 0;        
	result->dctl.dctl_opid = arg2.dctl_opid;
	result->dctl.dctl_plen = 0;       
	result->dctl.dctl_dtype = rt->dt;
	result->dctl.dctl_data.dctl_data_len = rt->len;
	result->dctl.dctl_data.dctl_data_val = rt->dbuf;

	free(rt);

	result->error.service_err = DIAMOND_SUCCESS;
	return 1;
}


bool_t
device_write_leaf_x_2_svc(u_int gen, dctl_x arg2, dctl_return_x *result,
			  struct svc_req *rqstp)
{
	int err;

	memset ((char *)result, 0, sizeof(*result));

	err = (*rpc_lstate->cb.wleaf_cb)
		(rpc_cstate->app_cookie, arg2.dctl_data.dctl_data_val,
		 (arg2.dctl_data.dctl_data_len - arg2.dctl_plen),
		 &(arg2.dctl_data.dctl_data_val[arg2.dctl_plen]));

	result->dctl.dctl_err = err;
	result->dctl.dctl_opid = arg2.dctl_opid;
	result->dctl.dctl_plen = 0;
	result->dctl.dctl_data.dctl_data_len = 0;
	result->dctl.dctl_data.dctl_data_val = NULL;

	if(!err) {
	  result->error.service_err = DIAMOND_SUCCESS;
	  return 1;
	}
	else {
	  result->error.service_err = DIAMOND_OPERR;
	  result->error.opcode_err = DIAMOND_OPCODE_FAILURE;
	  return 1;
	}
}


bool_t
device_list_nodes_x_2_svc(u_int gen, dctl_x arg2, dctl_return_x *result,
			  struct svc_req *rqstp)
{
	dctl_lnode_t *lt;

	memset ((char *)result, 0, sizeof(*result));

	lt = (rpc_lstate->cb.lnode_cb) (rpc_cstate->app_cookie,
					arg2.dctl_data.dctl_data_val);
	if(lt == NULL) {
	  result->error.service_err = DIAMOND_OPERR;
	  result->error.opcode_err = DIAMOND_FAILURE;
	  return 1;
	}

	result->dctl.dctl_err = lt->err;
	result->dctl.dctl_opid = arg2.dctl_opid;
	result->dctl.dctl_plen = 0;
	result->dctl.dctl_data.dctl_data_len = lt->num_ents * sizeof(dctl_entry_t);

	result->dctl.dctl_data.dctl_data_val = (char *)lt->ent_data; 

	free(lt);

	if(!result->dctl.dctl_err) {
	  result->error.service_err = DIAMOND_SUCCESS;
	  return 1;
	}
	else {
	  result->error.service_err = DIAMOND_OPERR;
	  result->error.opcode_err = DIAMOND_OPCODE_FAILURE;
	  return 1;
	}
}


bool_t
device_list_leafs_x_2_svc(u_int gen, dctl_x arg2, dctl_return_x *result,
			  struct svc_req *rqstp)
{
	dctl_lleaf_t *lt;

	memset ((char *)result, 0, sizeof(*result));

	lt = (rpc_lstate->cb.lleaf_cb) (rpc_cstate->app_cookie,
					arg2.dctl_data.dctl_data_val);

	result->dctl.dctl_err = lt->err;
	result->dctl.dctl_opid = arg2.dctl_opid;
	result->dctl.dctl_plen = 0;
	result->dctl.dctl_data.dctl_data_len = lt->num_ents * sizeof(dctl_entry_t);

	result->dctl.dctl_data.dctl_data_val = (char *)lt->ent_data;
	
	free(lt);

	if(!result->dctl.dctl_err) {
	  result->error.service_err = DIAMOND_SUCCESS;
	  return 1;
	}
	else {
	  result->error.service_err = DIAMOND_OPERR;
	  result->error.opcode_err = DIAMOND_OPCODE_FAILURE;
	  return 1;
	}
}


bool_t
device_set_exec_mode_x_2_svc(u_int gen, u_int mode, diamond_rc_t *result,
			     struct svc_req *rqstp)
{
	memset ((char *)result, 0, sizeof(*result));

	(rpc_lstate->cb.set_exec_mode_cb) (rpc_cstate->app_cookie, mode);

	result->service_err = DIAMOND_SUCCESS;
	return 1;
}


bool_t
device_set_user_state_x_2_svc(u_int gen, u_int state, diamond_rc_t *result,
			      struct svc_req *rqstp)
{
	memset ((char *)result, 0, sizeof(*result));

	(rpc_lstate->cb.set_user_state_cb) (rpc_cstate->app_cookie, state);

	result->service_err = DIAMOND_SUCCESS;
	return 1;
}

bool_t
device_set_obj_x_2_svc(u_int gen, sig_val_x arg2, diamond_rc_t *result,
		       struct svc_req *rqstp)
{
	char objpath[PATH_MAX];
	char * cache;
	char * sig_str;
	sig_val_t *sent_sig;

	memset ((char *)result, 0, sizeof(*result));

	sent_sig = (sig_val_t *)(arg2.sig_val_x_val);

	/*
	 * create a file for storing the searchlet library.
	 */
	umask(0000);

	cache = dconf_get_binary_cachedir();
	sig_str = sig_string(sent_sig);
	snprintf(objpath, PATH_MAX, SO_FORMAT, cache, sig_str);
	free(sig_str);
	free(cache);

	if (access(objpath, F_OK) == 0) {
	  int err;
	  err = (*rpc_lstate->cb.set_fobj_cb) (rpc_cstate->app_cookie, gen,
					       sent_sig);
	  if(err) {
	    result->service_err = DIAMOND_OPERR;
	    result->opcode_err = DIAMOND_OPCODE_FAILURE;//XXX: be more specific
	    return 1;
	  }

	} else {
	  rpc_cstate->pend_obj++;
	  result->service_err = DIAMOND_OPERR;
	  result->opcode_err = DIAMOND_OPCODE_FCACHEMISS;
	  return 1;
	}

	result->service_err = DIAMOND_SUCCESS;
	return 1;
}


bool_t
device_send_obj_x_2_svc(u_int gen, send_obj_x arg2, diamond_rc_t *result,
			struct svc_req *rqstp)
{
	int fd;
	char objname[PATH_MAX];
	sig_val_t calc_sig;
	sig_val_t *sent_sig;
	char * sig_str;
	char *cache;
	int err;

	memset ((char *)result, 0, sizeof(*result));

	sent_sig = (sig_val_t *)(arg2.obj_sig.sig_val_x_val);

	/* get name to store the object */ 	
	cache = dconf_get_binary_cachedir();
	sig_str = sig_string(sent_sig);
	snprintf(objname, PATH_MAX, SO_FORMAT, cache, sig_str);
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
		  result->service_err = DIAMOND_SUCCESS;
		  return 1;
		}
		result->service_err = DIAMOND_FAILEDSYSCALL;
		result->opcode_err = errno;
		return 1;
	} 
	if (writen(fd, arg2.obj_data.obj_data_val, arg2.obj_data.obj_data_len) !=  arg2.obj_data.obj_data_len) {
		perror("write buffer file"); 
		result->service_err = DIAMOND_FAILEDSYSCALL;
		result->opcode_err = errno;
		close(fd);
		return 1;
	}
	close(fd);
	file_release_lock(objname);

	err = (*rpc_lstate->cb.set_fobj_cb) (rpc_cstate->app_cookie, gen,
					     sent_sig);
	if(err) {
	  result->service_err = DIAMOND_OPERR;
	  result->opcode_err = DIAMOND_OPCODE_FAILURE; //XXX: be more specific
	  return 1;
	}

	rpc_cstate->pend_obj--;

	if((rpc_cstate->pend_obj== 0) && (rpc_cstate->have_start)) {
	    (*rpc_lstate->cb.start_cb) (rpc_cstate->app_cookie,
					rpc_cstate->start_gen);
	    rpc_cstate->have_start = 0;
	}

	result->service_err = DIAMOND_SUCCESS;
	return 1;
}


/* for anomaly detection */
bool_t
session_variables_get_x_2_svc(unsigned int gen,
			      diamond_session_var_list_return_x *result,
			      struct svc_req *rqstp) {
  int i;

  memset ((char *)result, 0, sizeof(*result));

  result->error.service_err = DIAMOND_SUCCESS;


  device_session_vars_t *vars =
	(*rpc_lstate->cb.get_session_vars_cb) (rpc_cstate->app_cookie, gen);

  if (vars == NULL) {
    result->error.service_err = DIAMOND_NOMEM;
    return 1;
  }


  // make into linked list
  diamond_session_var_list_x *first = NULL;
  diamond_session_var_list_x *prev = NULL;

  for (i = 0; i < vars->len; i++) {
    diamond_session_var_list_x *l = calloc(1, sizeof(diamond_session_var_list_x));
    if (l == NULL) {
      // this will fall through and send something to the client,
      // but also will let XDR free this structure for us
      result->error.service_err = DIAMOND_NOMEM;
      break;
    }

    if (i == 0) {
      first = l;
    } else {
      prev->next = l;
    }

    // load values
    l->name = vars->names[i]; // don't bother to strdup+free
    l->value = vars->values[i];

    //printf(" %d: \"%s\" -> %g\n", i, l->name, l->value);

    prev = l;
  }

  result->l = first;


  // free
  free(vars->names);
  free(vars->values);
  free(vars);

  // return
  return 1;
}

bool_t
session_variables_set_x_2_svc(unsigned int gen,
			      diamond_session_var_list_x list,
			      diamond_rc_t *result,
			      struct svc_req *rqstp)
{
  memset ((char *)result, 0, sizeof(*result));

  // fabricate the structure
  device_session_vars_t *vars = calloc(1, sizeof(device_session_vars_t));
  if (vars == NULL) {
    result->service_err = DIAMOND_NOMEM;
    return 1;
  }

  // count length
  int len = 0;
  diamond_session_var_list_x *first = &list;
  diamond_session_var_list_x *cur = first;

  while (cur != NULL) {
    cur = cur->next;
    len++;
  }

  // allocate some more
  vars->len = len;
  vars->names = calloc(len, sizeof(char *));
  vars->values = calloc(len, sizeof(double));

  if (vars->names == NULL || vars->values == NULL) {
    free(vars->names);
    free(vars->values);
    free(vars);
    result->service_err = DIAMOND_NOMEM;
    return 1;
  }

  // copy
  int i = 0;
  cur = first;
  while (cur != NULL) {
    vars->names[i] = strdup(cur->name);
    vars->values[i] = cur->value;

    cur = cur->next;
    i++;
  }

  // call
  (*rpc_lstate->cb.set_session_vars_cb) (rpc_cstate->app_cookie, gen, vars);

  // deallocate
  free(vars->names);
  free(vars->values);
  free(vars);

  // done
  result->service_err = DIAMOND_SUCCESS;
  return 1;
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
	  fprintf(stderr, "(storagestub) Reached EOF on control conn\n");
	  exit(EXIT_SUCCESS);	  
	}

	
	size_out = writen(cstate->rpc_fd, (void *)buf, size_in);
	if(size_out < 0) {
	  perror("write");
	  return;
	}
	
	if(size_in != size_out) {
	  fprintf(stderr, "Somehow lost bytes, from %d in to %d out!\n", 
		  size_in, size_out);
	  return;
	}

	return;
}
