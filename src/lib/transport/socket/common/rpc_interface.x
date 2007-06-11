/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 2
 *
 *  Copyright (c) 2002-2005 Intel Corporation
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
 *  Copyright (c) 2006-2007 Carnegie Mellon University
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */


enum diamond_service_error_t {
  /* Sun RPC failure codes should be subsumed here. */
  DIAMOND_SUCCESS = 0,
  DIAMOND_FAILURE = 1
};

enum diamond_opcode_error_t {
  DIAMOND_OPCODE_SUCCESS = 0,
  DIAMOND_OPCODE_FAILURE = 1
};

struct diamond_rc_t {
  diamond_service_error_t service_err;
  diamond_opcode_error_t  opcode_err;
};

struct stop_x {
  int		hs_objs_received;	
  int 		hs_objs_queued;
  int		hs_objs_read;
  int		hs_objs_uqueued;
  int		hs_objs_upresented;
};

/* "hyper" is the rpcgen type for a 64-bit quantity. */
typedef hyper    groupid_x;
typedef hyper    offload_x;
typedef opaque   spec_x<>;
typedef opaque   sig_val_x[16];  /* SIG_SIZE == 16, defined in 
				  * lib/libtools/sig_calc.h as 16 */

struct spec_file_x {
  sig_val_x sig;
  opaque    data<>;
};

struct filter_stats_x {
  string	fs_name<128>;             /* MAX_FILTER_NAME == 128 */
  int		fs_objs_processed;	  
  int		fs_objs_dropped;	 
  int		fs_objs_cache_dropped;
  int		fs_objs_cache_passed;
  int		fs_objs_compute;
  int		fs_hits_inter_session;
  int		fs_hits_inter_query;	
  int		fs_hits_intra_query;	
  hyper         fs_avg_exec_time;
};

struct dev_stats_x {
  int		    ds_objs_total;	   	
  int		    ds_objs_processed;	
  int		    ds_objs_dropped;	
  int		    ds_objs_nproc;		
  int		    ds_system_load;		
  hyper	            ds_avg_obj_time;
  filter_stats_x    ds_filter_stats<>;
};

struct devchar_x {
  unsigned int	dcs_isa;
  unsigned int	dcs_speed;
  hyper	        dcs_mem;
};

struct setlog_x{
  unsigned int log_level;
  unsigned int log_src;
};

struct dctl_x {
  unsigned int	dctl_err;  
  int           dctl_opid;      
  unsigned int  dctl_plen;
  unsigned int  dctl_dlen; 
  unsigned int  dctl_dtype;
  opaque 	dctl_data<>;
};

struct send_obj_x {
  sig_val_t	obj_sig;
  uint32_t	obj_len;
  opaque        obj_data<>; 
};

struct blob_x {
  string blob_name<>;
  opaque blob_data<>;	/* the data with the name followed by blob */
};

program OPENDIAMOND_PROG {
  version OPENDIAMOND_VERS {


    /* Client calls. */

    diamond_rc_t device_start_x(int) = 1;
    diamond_rc_t device_stop_x(int, stop_x) = 2;
    diamond_rc_t device_terminate_x(int) = 3;
    diamond_rc_t device_clear_gids_x(int) = 4;
    diamond_rc_t device_new_gid_x(int, groupid_x) = 5;
    diamond_rc_t device_set_offload_x(int, offload_x) = 6;
    diamond_rc_t device_set_spec_x(int, spec_file_x) = 7;
    diamond_rc_t device_set_log_x(setlog_x) = 9;
    diamond_rc_t device_write_leaf_x(dctl_x) = 10;
    diamond_rc_t device_read_leaf_x(dctl_x) = 11;
    diamond_rc_t device_list_nodes_x(dctl_x) = 12;
    diamond_rc_t device_list_leafs_x(dctl_x) = 13;
    diamond_rc_t device_set_blob_x(int, blob_x) = 14;
    diamond_rc_t device_set_exec_mode_x(int, unsigned int) = 15;
    diamond_rc_t device_set_user_state_x(int, unsigned int) = 16;
    diamond_rc_t request_chars_x(void) = 17;
    diamond_rc_t request_stats_x(void) = 18;


    /* These three calls are related respectively by:
     * client call->server callback->client callback
     * Since we don't want to have to run a TI-RPC server in the client,
     * the first call will become synchronous, and the latter two will
     * merge into a new call. */

    diamond_rc_t device_set_lib_x(int, sig_val_x) = 8;
    //diamond_rc_t sstub_get_obj_x(sig_val_x) = 21;
    //diamond_rc_t send_obj_x(int, send_obj_x) = 26;
    
    diamond_rc_t device_send_obj_x(int, send_obj_x) = 26;


    /* Server callbacks. These calls will disappear since they are
     * supposed to be synchronous anyway and we don't want to have a
     * TI-RPC server handling these in the Diamond client. */

    diamond_rc_t sstub_send_stats_x(dev_stats_x) = 19;
    diamond_rc_t sstub_send_dev_char_x(dev_char_x) = 20;
    diamond_rc_t sstub_wleaf_response_x(dctl_x) = 22;
    diamond_rc_t sstub_rleaf_response_x(dctl_x) = 23;
    diamond_rc_t sstub_lleaf_response_x(dctl_x) = 24;
    diamond_rc_t sstub_lnode_response_x(dctl_x) = 25;

  } = 2;
} = 0x2405E65E;  /* The leading "0x2" is required for "static"
		  * programs that do not use portmap/rpcbind. The last
		  * seven digits were randomly generated. */
