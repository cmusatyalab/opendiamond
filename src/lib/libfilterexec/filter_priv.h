#ifndef _FILTER_PRIV_H_
#define _FILTER_PRIV_H_

#include "rtimer.h"
#include "rgraph.h"
#include "lib_filter.h"		/* for filter_proto */
#include "queue.h"
#include "consts.h"

/*
 * This the the header file used to keep track of the 
 * filter state for each of the filters in the current
 * application.
 */

typedef enum filter_output_type {
	FO_UNMODIFIED=0,	/* default */
	FO_NEW,
	FO_CLONE,
	FO_COPY_ATTR
} filter_output_type_t;


/* name is set to a strdup'd string when reading filter
 * info. resolve_filter_deps will change pointer to filter*.
 * not needed elsewhere?
 */
//struct filter_info;
typedef struct filter_dep_t {
	char *name;
	//struct filter_info *filter;
	//TAILQ_ENTRY(filter_dep_t) link;
} filter_dep_t;


typedef  uint8_t filter_id_t;
#define INVALID_FILTER_ID   0xFF

typedef union {
    void *      ptr;
    filter_id_t fid;
} fid_union_t;

#define FILTER_ID(x)    ((fid_union_t)(x))

typedef struct filter_info {
	/*
	 * general filter info (not device-specific)
	 */
	char			        fi_name[MAX_FILTER_NAME];
	char			        fi_fname[MAX_FILTER_FUNC_NAME];
	filter_proto            fi_fp;
	int			            fi_threshold;
	int			            fi_merit;
	int			            fi_numargs;
	char *			        fi_args[MAX_NUM_ARGS];
	struct filter_info *	fi_next;
    filter_id_t             fi_filterid;    /* id of this filter */
    filter_id_t             fi_nextfilter;  /* next filter to run */


	/* dependency info */
	int                     fi_color; /* used by dfs */
	int                     fi_depcount;
	filter_dep_t            fi_deps[MAX_NUM_DEPS];
	node_t                 *fi_gnode;

	/* input characteristics */
	int                     fi_blocksize_in;

	/* output characteristics */
	int                     fi_blocksize_out;
	filter_output_type_t    fi_type_out;

	/*
	 * statistics. these should be local to each device.
	 */
	int			            fi_called;   /* # of times called */
	int			            fi_drop;     /* # times below threshold */
	int			            fi_pass;     /* # times above threshold */
	rtime_t                 fi_time_ns;  /* total time used */
} filter_info_t;


typedef struct filter_prob {
    LIST_ENTRY(filter_prob) prob_link;
    int             num_pass;       /* # of times this combination passed */
    int             num_exec;       /* # of times this combination was run */
    int             num_prev;       /* # of previus filters run */
    filter_id_t     cur_fid;        /* current filter id */
    filter_id_t     prev_id[0];     /* list of previous IDs */
} filter_prob_t;


#define FILTER_PROB_SIZE(x)     \
        (sizeof(filter_prob_t)+((x)* sizeof(filter_id_t)))

/* must be power of 2 */
#define PROB_HASH_BUCKETS   64

/*
 * This is the structure that holds all the data.
 */
struct filter_data {
    int                 fd_num_filters;
    filter_id_t         fd_first_filter;
    filter_id_t         fd_max_filters;
    filter_id_t         fd_app_id;
    LIST_HEAD(prob_hash, filter_prob)   fd_prob_hash[PROB_HASH_BUCKETS];
    filter_info_t       fd_filters[0];
};


int     read_filter_spec(char *spec_name, filter_data_t **fdp);
void    fexec_update_prob(filter_data_t *fdata, filter_id_t cur_filt,
                filter_id_t *prev_list, int num_prev, int pass);


#endif	/* ifndef _FILTER_PRIV_H_ */
