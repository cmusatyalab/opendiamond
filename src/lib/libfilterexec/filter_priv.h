#ifndef _FILTER_PRIV_H_
#define _FILTER_PRIV_H_

#include <stdint.h>

#include "rtimer.h"
#include "rgraph.h"
#include "lib_filter.h"         /* for filter_proto */
#include "queue.h"
#include "consts.h"
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
typedef struct filter_dep_t {
    char           *name;
    // struct filter_info *filter;
    // TAILQ_ENTRY(filter_dep_t) link;
} filter_dep_t;


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
    filter_init_proto fi_init_fp;
    filter_eval_proto fi_eval_fp;
    filter_fini_proto fi_fini_fp;
    int             fi_threshold;
    int             fi_merit;
    int             fi_numargs;
    int             fi_maxargs;
    char          **fi_arglist;
    filter_id_t     fi_filterid;    /* id of this filter */
    void           *fi_filt_arg;    /* associated argument data */

    int             fi_blob_len;    /* associated blob len */
    void           *fi_blob_data;   /* associated blob of data */

    /*
     * dependency info 
     */
    int             fi_color;   /* used by dfs */
    int             fi_depcount;
    filter_dep_t    fi_deps[MAX_NUM_DEPS];
    node_t         *fi_gnode;

    /*
     * bypass information, the fi_bpcnt keeps track of where we are in the
     * loop.  We logically process fi_bprun/fi_bpmax of these objects. 
     */

    int             fi_bpthresh;

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
    int             fi_bypassed;    /* # of times we would have run */
    int             fi_drop;    /* # times below threshold */
    int             fi_pass;    /* # times above threshold */
    rtime_t         fi_time_ns; /* total time used */
    int64_t         fi_added_bytes; /* XXX debug */
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
    int             obj_ns_pos; /* current insertion point */

    int             obj_counter;    /* used to synchronize monitoring output
                                     * (filter_exec) */

    filter_info_t   fd_filters[0];  /* variable size struct */
};


#define	ARG_CHUNK	16

int             read_filter_spec(char *spec_name, struct filter_data **fdp);
void            fexec_update_prob(struct filter_data *fdata,
                                  filter_id_t cur_filt,
                                  const filter_id_t * prev_list, int num_prev,
                                  int pass);


filter_prob_t  *fexec_lookup_prob(filter_data_t * fdata, filter_id_t cur_filt,
                                  int num_prev, const filter_id_t * slist);

int             fexec_compute_cost(filter_data_t * fdata,
                                   permutation_t * perm, int gen, int indep,
                                   float *cost);

int             fexec_estimate_cost(filter_data_t * fdata,
                                   permutation_t * perm, int gen, int indep,
                                   float *cost);


#endif                          /* ifndef _FILTER_PRIV_H_ */
