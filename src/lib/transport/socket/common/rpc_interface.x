/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 2
 *
 *  Copyright (c) 2007 Carnegie Mellon University
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
  DIAMOND_FAILEDSYSCALL,
  DIAMOND_NOMEM,
  DIAMOND_OPERR,
  DIAMOND_FAILURE
};

enum diamond_opcode_error_t {
  DIAMOND_OPCODE_SUCCESS = 0,
  DIAMOND_OPCODE_FCACHEMISS,
  DIAMOND_OPCODE_FAILURE
};

struct diamond_rc_t {
  diamond_service_error_t service_err;
  diamond_opcode_error_t  opcode_err;
};

struct stop_x {
  int host_objs_received;
  int host_objs_queued;
  int host_objs_read;
  int app_objs_queued;
  int app_objs_presented;
};

/* "hyper" is the rpcgen type for a 64-bit quantity. */
typedef hyper    groupid_x;
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

struct dctl_x {
  unsigned int	dctl_err;  
  int           dctl_opid;      
  unsigned int  dctl_plen;
  unsigned int  dctl_dtype;
  opaque 	dctl_data<>;
};

struct send_obj_x {
  sig_val_x	obj_sig;
  opaque        obj_data<>; 
};

struct blob_x {
  opaque blob_name<>;
  opaque blob_data<>;	/* the data with the name followed by blob */
};

struct send_stats_return_x {
  diamond_rc_t error;
  dev_stats_x stats;
};

struct request_chars_return_x {
  diamond_rc_t error;
  dev_char_x chars;
};

struct dctl_return_x {
  diamond_rc_t error;
  dctl_x dctl;
};

program OPENDIAMOND_PROG {
  version OPENDIAMOND_VERS {

    /* Client calls. */

    diamond_rc_t            device_start_x(unsigned int gen) = 1;
    diamond_rc_t            device_stop_x(unsigned int gen, stop_x) = 2;
    diamond_rc_t            device_terminate_x(unsigned int gen) = 3;
    diamond_rc_t            device_clear_gids_x(unsigned int gen) = 4;
    diamond_rc_t            device_new_gid_x(unsigned int gen, groupid_x) = 5;
    diamond_rc_t            device_set_spec_x(unsigned int gen, spec_file_x) = 6;
    dctl_return_x           device_write_leaf_x(unsigned int gen, dctl_x) = 7;
    dctl_return_x           device_read_leaf_x(unsigned int gen, dctl_x) = 8;
    dctl_return_x           device_list_nodes_x(unsigned int gen, dctl_x) = 9;
    dctl_return_x           device_list_leafs_x(unsigned int gen, dctl_x) = 10;
    diamond_rc_t            device_set_blob_x(unsigned int gen, blob_x) = 11;
    diamond_rc_t            device_set_exec_mode_x(unsigned int gen, 
						   unsigned int mode) = 12;
    diamond_rc_t            device_set_user_state_x(unsigned int gen, 
						    unsigned int state) = 13;
    request_chars_return_x  request_chars_x(unsigned int gen) = 14;
    request_stats_return_x  request_stats_x(unsigned int gen) = 15;


    /* The filter caching calls are related respectively by:
     * client call(SET_OBJ) -> server callback(GET_OBJ) -> 
     *   client callback(SEND_OBJ)
     *
     * Since we don't want to have to run a TI-RPC server in the
     * client, the first two calls will become a single synchronous
     * call and the last will become a new call. */

    diamond_rc_t device_set_obj_x(unsigned int gen, sig_val_x) = 16;
    diamond_rc_t device_send_obj_x(unsigned int gen, send_obj_x) = 17;

  } = 2;
} = 0x2405E65E;  /* The leading "0x2" is required for "static"
		  * programs that do not use portmap/rpcbind. The last
		  * seven digits were randomly generated. */
