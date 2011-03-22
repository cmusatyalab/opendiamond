/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 4
 *
 *  Copyright (c) 2002-2005 Intel Corporation
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#ifndef _FILTER_PRIV_H_
#define _FILTER_PRIV_H_

#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include <sys/queue.h>

#include "rtimer.h"
#include "rgraph.h"
#include "lib_filter.h"         /* for filter_proto */
#include "diamond_consts.h"
#include "rcomb.h"


/*
 * This the the header file used to keep track of the 
 * filter state for each of the filters in the current
 * application.
 */

typedef enum filter_output_type {
    FO_UNMODIFIED = 0,          /* default */
    FO_NEW,
    FO_CLONE,
    FO_COPY_ATTR
} filter_output_type_t;


/*
 * name is set to a strdup'd string when reading filter info.
 * resolve_filter_deps will change pointer to filter*. not needed elsewhere? 
 */
// struct filter_info;
typedef struct filter_dep_t
{
	char           *name;
	// struct filter_info *filter;
	// TAILQ_ENTRY(filter_dep_t) link;
}
filter_dep_t;


/*
 * These are some definitions related to the filter id
 * data type.
 */
#define FILTER_ID_MASK  0x000000FF
#define FILTER_ID(x)    ((filter_id_t)(((uint32_t)(x))&FILTER_ID_MASK))

typedef uint8_t filter_id_t;
#define INVALID_FILTER_ID   0xFF


typedef struct filter_info {
	/*
	 * general filter info (not device-specific)
	 */
	char            fi_name[MAX_FILTER_NAME];
	char            fi_eval_name[MAX_FILTER_FUNC_NAME];
	char            fi_init_name[MAX_FILTER_FUNC_NAME];
	char            fi_fini_name[MAX_FILTER_FUNC_NAME];
	char            fi_signature[SIG_SIZE];
	FILE           *fi_in_from_filter;
	FILE           *fi_out_to_filter;
	int             fi_threshold;
	int             fi_merit;
	int             fi_numargs;
	int             fi_maxargs;
	char          **fi_arglist;
	filter_id_t     fi_filterid;    /* id of this filter */
	bool            fi_is_initialized;  /* for lazy calling of fi_init_fp */
	bool            fi_init_success; /* was the init successful yet? */

	int             fi_blob_len;    /* associated blob len */
	void           *fi_blob_data;   /* associated blob of data */
	sig_val_t	fi_blob_sig;	/* checksum of the blob data */

	/*
	 * dependency info 
	 */
	int             fi_color;   /* used by dfs */
	int             fi_depcount;
	filter_dep_t    fi_deps[MAX_NUM_DEPS];
	node_t         *fi_gnode;

	/*
	 * input characteristics 
	 */
	int             fi_blocksize_in;

	/*
	 * output characteristics 
	 */
	int             fi_blocksize_out;
	filter_output_type_t fi_type_out;

	/*
	 * statistics. these should be local to each device.
	 */
	int             fi_called;  /* # of times called */
	int             fi_drop;    /* # times below threshold */
	int             fi_pass;    /* # times above threshold */
	int             fi_error;   /* # errors running filter */
	rtime_t         fi_time_ns; /* total time used */
	int64_t         fi_added_bytes; /* XXX debug */
	sig_val_t	fi_sig;
	int	    	fi_cache_drop; /* # of objs dropped through cache lookup */
	int		fi_cache_pass; /* # of objs skipped by using cache */
	int		fi_compute;    /* # of objs evaluated */
	int		fi_hits_inter_session;	/* hits computed before this session */
	int		fi_hits_inter_query;	/* hits computed in session, before query */
	int		fi_hits_intra_query;	/* hits computed this query */
} filter_info_t;


typedef struct filter_prob {
	LIST_ENTRY(filter_prob) prob_link;
	int             num_pass;   /* # of times this combination passed */
	int             num_exec;   /* # of times this combination was run */
	int             num_prev;   /* # of previus filters run */
	filter_id_t     cur_fid;    /* current filter id */
	filter_id_t     prev_id[0]; /* list of previous IDs */
} filter_prob_t;


#define FILTER_PROB_SIZE(x)     \
        (sizeof(filter_prob_t)+((x)* sizeof(filter_id_t)))

/*
 * must be power of 2 
 */
#define PROB_HASH_BUCKETS   64

/*
 * size of history 
 */
#define STAT_WINDOW 1024

#define FCODE_INCREMENT	10

typedef struct fcode_info {
	sig_val_t	code_sig;
	char *		code_name;
} fcode_info_t;

/*
 * This is the structure that holds all the data.
 * initialized in filter_spec.l:read_filter_spec -RW
 */
struct filter_data {
	int             fd_num_filters;
	filter_id_t     fd_max_filters;
	filter_id_t     fd_app_id;
	LIST_HEAD(prob_hash, filter_prob) fd_prob_hash[PROB_HASH_BUCKETS];
	permutation_t  *fd_perm;    /* current permutation */
	partial_order_t *fd_po;     /* the partial ordering of filters */

	/*
	 * stats to keep track of the loading weighted moving averages 
	 */
	double          fd_avg_wall;
	double          fd_avg_exec;

	time_t          obj_ns[STAT_WINDOW];    /* data */
	int             obj_ns_valid;   /* number of valid entries */
	int             obj_ns_pos; 	/* current insertion point */

	int             obj_counter;    /* used to synchronize monitoring output
				     	 * (filter_exec) 
					 */
	int		max_codes;
	int		num_codes;
	sig_val_t	spec_sig;
	int		full_eval;
	fcode_info_t *	code_info;
	
	filter_info_t   fd_filters[0];  /* variable size struct */
};


#define	ARG_CHUNK	16

int             read_filter_spec(char *spec_name, struct filter_data **fdp);
void            fexec_update_prob(struct filter_data *fdata,
                                  filter_id_t cur_filt,
                                  const filter_id_t * prev_list, int num_prev,
                                  bool pass);


filter_prob_t  *fexec_lookup_prob(filter_data_t * fdata, filter_id_t cur_filt,
                                  int num_prev, const filter_id_t * slist);

int             fexec_compute_cost(filter_data_t * fdata,
                                   permutation_t * perm, int gen, int indep,
                                   float *cost);

int             fexec_estimate_cost(filter_data_t * fdata,
                                    permutation_t * perm, int gen, int indep,
                                    float *cost);

void		update_filter_order(filter_data_t * fdata, const permutation_t * perm);


void	    	optimize_filter_order(filter_data_t * fdata, 
			opt_policy_t * policy);
double	    	tv_diff(struct timeval *end, struct timeval *start);
int             run_eval_server(FILE *in, FILE *out, obj_data_t *obj_handle, filter_info_t *cur_filt);
void            fexec_clear_stats(filter_data_t * fdata);

int             fexec_estimate_cost(filter_data_t * fdata,
				    permutation_t * perm, int gen, int indep,
				    float *cost);
void            fexec_possibly_init_filter(filter_info_t *cur_filt,
					   int num_codes,
					   fcode_info_t *fcodes);


int fexec_ref_attr(lf_obj_handle_t ohandle, const char *name,
			 size_t *len, unsigned char **data);
int fexec_write_attr(lf_obj_handle_t ohandle, char *name, size_t len,
			   unsigned char *data);
int fexec_omit_attr(lf_obj_handle_t ohandle, char *name);
int fexec_get_session_variables(lf_obj_handle_t ohandle,
				      char **names,
				      double *results);
int fexec_update_session_variables(lf_obj_handle_t ohandle,
					 char **names,
					 double *values);


extern filter_info_t *fexec_active_filter;

/*
 * update at your own risk! 
 */
extern enum policy_type_t filter_exec_current_policy;


#endif                    /* ! _FILTER_PRIV_H_ */
