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


typedef struct filter_info {
	/*
	 * general filter info (not device-specific)
	 */
	char			fi_name[MAX_FILTER_NAME];
	char			fi_fname[MAX_FILTER_FUNC_NAME];
	filter_proto		fi_fp;
	int			fi_threshold;
	int			fi_merit;
	int			fi_numargs;
	char *			fi_args[MAX_NUM_ARGS];
	struct filter_info *	fi_next;

	/* dependency info */
	int                     fi_color; /* used by dfs */
	int                     fi_depcount;
	filter_dep_t            fi_deps[MAX_NUM_DEPS];
	//TAILQ_HEAD(deps, filter_dep_t)  fi_deps;
	node_t                 *fi_gnode;

	/* input characteristics */
	int                     fi_blocksize_in;

	/* output characteristics */
	int                     fi_blocksize_out;
	filter_output_type_t    fi_type_out;

	/*
	 * statistics. these should be local to each device.
	 */
	int			fi_called;   /* # of times called */
	int			fi_drop;     /* # times below threshold */
	int			fi_pass;     /* # times above threshold */
	rtime_t                 fi_time_ns;  /* total time used */

} filter_info_t;


#endif	/* ifndef _FILTER_PRIV_H_ */
