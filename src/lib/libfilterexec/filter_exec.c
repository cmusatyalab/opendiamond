
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>
#include <assert.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>

#include "lib_od.h"
#include "lib_dctl.h"
#include "dctl_common.h"
#include "lib_odisk.h"
#include "lib_searchlet.h"
#include "lib_log.h"
#include "attr.h"
#include "filter_exec.h"
#include "filter_priv.h"
#include "rtimer.h"
#include "rgraph.h"
#include "fexec_stats.h"
#include "fexec_opt.h"
#include "fexec_bypass.h"

/* #define VERBOSE 1 */
/* #define SIM_REPORT */

/*
 * Some state to keep track of the active filter. XXX
 */
static filter_info_t  		*active_filter = NULL;
static char			*no_filter = "None";




/* ********************************************************************** */
/* filter optimization policy defs */
/* ********************************************************************** */

typedef struct opt_policy_t {
  enum policy_type_t policy;
  void *(*p_new)(struct filter_data *);
  void  (*p_delete)(void *context);
  int   (*p_optimize)(void *context, struct filter_data *);
  void *p_context;
  int  exploit;			/* if we are in exploit mode */
} opt_policy_t;

struct filter_exec_t filter_exec = {
  NULL_POLICY
};

// int CURRENT_POLICY = NULL_POLICY;
//int CURRENT_POLICY = HILL_CLIMB_POLICY;
//int CURRENT_POLICY = BEST_FIRST_POLICY;

/* order here should match enum policy_type_t */
static opt_policy_t policy_arr[] = {
  { NULL_POLICY, NULL, NULL, NULL, NULL },
  { HILL_CLIMB_POLICY, hill_climb_new, hill_climb_delete, hill_climb_optimize, NULL },
  { BEST_FIRST_POLICY, best_first_new, best_first_delete, best_first_optimize, NULL },
  //{ INDEP_POLICY, indep_new, indep_delete, indep_optimize, NULL },
  { INDEP_POLICY, indep_new, best_first_delete, best_first_optimize, NULL },
  { RANDOM_POLICY, random_new, NULL, NULL, NULL },
  { STATIC_POLICY, static_new, NULL, NULL, NULL },
  { NULL_POLICY, NULL, NULL, NULL, NULL }
};


/*
 * Global state for the filter init code.
 */
int	fexec_fixed_split = 0;	/* we use a fixed partioning if this is 1 */
int	fexec_fixed_ratio = 0;	/* percentage for a fixed partitioning */
int	fexec_cpu_slowdown = 0;	/* percentage slowdown for CPU  */

char 		ratio[40];
char 		pid_str[40];

int
fexec_set_slowdown(void *cookie, int data_len, char *val)
{
	uint32_t	data;
	pid_t		new_pid;
	int			err;
	pid_t		my_pid;
	

	data = *(uint32_t *)val;

	fprintf(stderr, "slowdown !!!! \n");
	if (fexec_cpu_slowdown != 0) {
		fprintf(stderr, "slowdonw already set !!!! \n");
		return(EAGAIN);
	}
	
	if (data == 0) {
		fprintf(stderr, "slowdonw no data  !!!! \n");
		return(0);
	}

	if (data > 90) {
		fprintf(stderr, "slowdonw out of range  !!!! \n");
		return(EINVAL);
	}

	fexec_cpu_slowdown = data;

	my_pid = getpid();

	fprintf(stderr, "my pid %d \n", my_pid);

	new_pid = fork();
	if (new_pid == 0) {
		sprintf(ratio, "%d", data);
		sprintf(pid_str, "%d", my_pid);
		err = execlp("/home/diamond/bin/slowdown", "slowdown", "-r", ratio, 
			"-p", pid_str, NULL);
		if (err) {
			perror("exec failed:");
		}
	}

	fprintf(stderr, "child pid %d \n", new_pid);
	return(0);	

}

/* ********************************************************************** */

void
fexec_system_init() 
{
	unsigned int seed;
	int fd;
	int rbytes;

	/* it will default to STD if this doesnt work */
	rtimer_system_init(RTIMER_PAPI); 

	dctl_register_leaf(DEV_FEXEC_PATH, "fixed_split", DCTL_DT_UINT32,
		dctl_read_uint32, dctl_write_uint32, &fexec_fixed_split);

	dctl_register_leaf(DEV_FEXEC_PATH, "fixed_ratio", DCTL_DT_UINT32,
		dctl_read_uint32, dctl_write_uint32, &fexec_fixed_ratio);

	dctl_register_leaf(DEV_FEXEC_PATH, "cpu_slowdown", DCTL_DT_UINT32,
		dctl_read_uint32, fexec_set_slowdown, &fexec_cpu_slowdown);

#ifdef VERBOSE
  	fprintf(stderr, "fexec_system_init: policy = %d\n", 
		filter_exec.current_policy);
#endif


	/*
	 * Initialize the random number generator using a
     * seed from urandom.  We need to see some randomness
	 * across runs. 
     */
	fd = open("/dev/urandom", O_RDONLY);
	assert(fd != -1);
	rbytes = read(fd, (void *)&seed, sizeof(seed));
	assert(rbytes == sizeof(seed));

	srandom(seed);
}


char *
fexec_cur_filtname()
{
	if (active_filter != NULL) {
		return(active_filter->fi_name);
	} else {
		return(no_filter);
	}
}
		

int
fexec_init_search(filter_data_t *fdata)
{
	void *          	data;
	filter_info_t *		cur_filt;
	filter_id_t     	fid;
	int					err;

	/* clean up the stats */
	fexec_clear_stats(fdata);

	/* Go through all the filters and call the init function */
	for (fid = 0; fid < fdata->fd_num_filters; fid++) {
		cur_filt = &fdata->fd_filters[fid];
		if (fid == fdata->fd_app_id) {
			continue;
		}

	    err = cur_filt->fi_init_fp(cur_filt->fi_numargs,
			cur_filt->fi_args, cur_filt->fi_blob_len, 
			cur_filt->fi_blob_data, &data);

		if (err != 0) {
			/* XXXX what now */
			assert(0);
		}

		cur_filt->fi_filt_arg  = data;
    } 
    return(0);
}

int
fexec_term_search(filter_data_t *fdata)
{
	filter_info_t *		cur_filt;
	filter_id_t     	fid;
	int					err;

	/* Go through all the filters and call the init function */
	for (fid = 0; fid < fdata->fd_num_filters; fid++) {
		cur_filt = &fdata->fd_filters[fid];
		if (fid == fdata->fd_app_id) {
			continue;
		}

	    err = cur_filt->fi_fini_fp(cur_filt->fi_filt_arg);
		if (err != 0) {
			/* XXXX what now */
			assert(0);
		}
    } 
    return(0);
} 
static int
load_filter_lib(char *lib_name, filter_data_t *fdata)
{
	void *          	handle;
	filter_info_t *		cur_filt;
	filter_eval_proto	fe;
	filter_init_proto	fi;
	filter_fini_proto	ff;
	filter_id_t     	fid;
	char *		    	error;

	handle = dlopen(lib_name, RTLD_NOW);
	if (!handle) {
		/* XXX error log */
		fputs(dlerror(), stderr);
		exit (1);
	}

#ifdef VERBOSE
	fprintf(stderr, "loaded %s at %p\n", lib_name, handle);
#endif

	/* XXX keep the handle somewhere */
	for (fid = 0; fid < fdata->fd_num_filters; fid++) {
		cur_filt = &fdata->fd_filters[fid];
		if (fid == fdata->fd_app_id) {
			continue;
		}

		fe = dlsym(handle, cur_filt->fi_eval_name);
		if ((error = dlerror()) != NULL) {
			/* XXX error handling */
			fprintf(stderr, "%s on <%s> \n", error, cur_filt->fi_eval_name);
			return(ENOENT);
		}
		cur_filt->fi_eval_fp = fe;



		fi = dlsym(handle, cur_filt->fi_init_name);
		if ((error = dlerror()) != NULL) {
			/* XXX error handling */
			fprintf(stderr, "%s on <%s> \n", error, cur_filt->fi_init_name);
			return(ENOENT);
		}
		cur_filt->fi_init_fp = fi;

		ff = dlsym(handle, cur_filt->fi_fini_name);
		if ((error = dlerror()) != NULL) {
			/* XXX error handling */
			fprintf(stderr, "%s on <%s> \n", error, cur_filt->fi_fini_name);
			return(ENOENT);
		}
		cur_filt->fi_fini_fp = ff;



#ifdef VERBOSE
		fprintf(stderr, "filter %d (%s): resolved.\n", fid, cur_filt->fi_name);
#endif
    }
    
    return(0);
}

/*
 * This function goes through the filters and makes sure all the
 * required values have been specified.  If we are missing some
 * fields then we return EINVAL.
 */
static int
verify_filters(filter_data_t *fdata)
{

    filter_id_t     fid;

	log_message(LOGT_FILT, LOGL_TRACE, "verify_filters(): starting");


	/*
	 * Make sure we have at least 1 filter defined.
	 */
	if (fdata->fd_num_filters == 0) {
		log_message(LOGT_FILT, LOGL_ERR, 
				"verify_filters(): no filters");
        printf("num filters is 0 \n");
		return(EINVAL);
	}



	/*
	 * Loop over all the filters and makes sure some minimum requirements
	 * are checked.
	 */
	for (fid = 0; fid < fdata->fd_num_filters; fid++) {
		if (fid == fdata->fd_app_id) {
			continue;
		}
		/*
		 * Make sure the filter exisits and it is at least
		 * 1 character.
		 */
		if (strlen(fdata->fd_filters[fid].fi_name) == 0) {
			log_message(LOGT_FILT, LOGL_ERR, 
				    "verify_filters(): no filter name");
			printf("bad name len  on %d \n", fid);
			return(EINVAL);
		}

		/*
		 * Make sure the threshold is defined.
		 */
		if (fdata->fd_filters[fid].fi_threshold  == -1) {
			log_message(LOGT_FILT, LOGL_ERR, 
				    "verify_filters(): no threshold");
			printf("bad threshold on %s \n", fdata->fd_filters[fid].fi_name);
			return(EINVAL);
		}
	}
	
	log_message(LOGT_FILT, LOGL_TRACE, "verify_filters(): complete");

	return(0);
}


static filter_id_t 
find_filter_id(filter_data_t *fdata, const char *name)
{
    filter_id_t i;

    for (i=0; i < fdata->fd_num_filters; i++) {
        if(strcmp(fdata->fd_filters[i].fi_name, name) == 0) {
            return(i);
        }
	}
	return(INVALID_FILTER_ID);
}

static void
print_filter_list(char *tag, filter_data_t *fdata)
{
	int         i;

	fprintf(stderr, "%s:", tag);
	if(fdata->fd_num_filters == 0) {
		fprintf(stderr, "<null>");
		return;
	}

	for (i = 0; i < fdata->fd_num_filters; i++) {
		fprintf(stderr, " %s", fdata->fd_filters[i].fi_name);
	}
	fprintf(stderr, "\n");
}



/* build a label in buf (potential overflow error) using the filter
 * name and attributes. dependency - \\n format used by daVinci. (this
 * should have been pushed into rgraph.c, but it's ok here) */
static void
build_label(char *buf, filter_info_t *fil) {
	int i;
	int width = strlen(fil->fi_name) * 2;

	buf[0] = '\0';
	sprintf(buf, "%s\\n", fil->fi_name);
	for(i=0; i<fil->fi_numargs; i++) {
		strcat(buf, " ");
		strcat(buf, fil->fi_args[i]);
		//strcat(buf, "\\n");
		if(strlen(buf)>width) {	/* too long */
			strcat(buf, "...");
			break;
		}
	}
}

/* static permutation_t * */
/* fdata_new_perm(filter_data_t *fdata) { */
/* 	filter_id_t fid; */
/* 	int pos = 0; */
/* 	permutation_t *cur_perm; */
	
/* 	cur_perm = pmNew(fdata->fd_num_filters); */
/* 	for(fid = fdata->fd_first_filter; */
/* 	    fid != INVALID_FILTER_ID; */
/* 	    fid = fdata->fd_filters[fid].fi_nextfilter) { */
/* 		pmSetElt(cur_perm, pos++, fid); */
/* 	} */

/* 	return cur_perm; */
/* } */


/*
 * figure out the filter execution graph from the dependency
 * information. generate the initial ordering.
 * this function also exports the graphs.
 * RETURNS a pointer to the list for the initial order, or NULL if error
 */
static int
resolve_filter_deps(filter_data_t *fdata)
{
	filter_info_t *cur_filter;
	int i;
	int id, tempid;
	//int lastid;
	graph_t graph;
	node_t *np;
	char *filename = "filters";
	node_t *src_node;

	/* 
	 * create the graph of filter nodes
	 */
	gInit(&graph);
	src_node = gNewNode(&graph, "DATASRC");

	for (id = 0; id < fdata->fd_num_filters; id++) {
		char buf[BUFSIZ];
		cur_filter = &fdata->fd_filters[id];
		build_label(buf, cur_filter);
		cur_filter->fi_gnode = gNewNode(&graph, buf);
		//cur_filter->fi_gnode->data = (void *)id;
		cur_filter->fi_gnode->data = (void *)cur_filter;
#ifdef VERBOSE
		printf("adding to graph: %s\n", cur_filter->fi_name);
#endif
		cur_filter->fi_gnode->val = cur_filter->fi_merit;
	}


	/* 
	 * add the dependencies
	 */
	
	fdata->fd_po = poNew(fdata->fd_num_filters);

	for (id = 0; id < fdata->fd_num_filters; id++) {
		cur_filter = &fdata->fd_filters[id];
#ifdef VERBOSE
		fprintf(stderr, "resolving dependencies for %s\n", 
			cur_filter->fi_name);
#endif

		/* everyone depends on source (at least transitively) */
		if(cur_filter->fi_depcount == 0) {
			gAddEdge(&graph, src_node, cur_filter->fi_gnode);
		}

		/* add edges; note direction reversed */
		for(i=0; i<cur_filter->fi_depcount; i++) {
			tempid = find_filter_id(fdata, cur_filter->fi_deps[i].name);
			if (tempid == INVALID_FILTER_ID) {
				fprintf(stderr, "could not resolve filter <%s>"
					" needed by <%s>\n",
					cur_filter->fi_deps[i].name,
					cur_filter->fi_name);
				return(EINVAL);
			}
#ifdef	XXX
			free(cur_filter->fi_deps[i].name);
			cur_filter->fi_deps[i].name = NULL; /* XXX */
#endif
			//cur_filter->fi_deps[i].filter = tmp;
			gAddEdge(&graph, fdata->fd_filters[tempid].fi_gnode, 
                            cur_filter->fi_gnode);

			/* also build up a partial_order */
			poSetOrder(fdata->fd_po, id, tempid, PO_GT);
		}
		poSetOrder(fdata->fd_po, id, id, PO_EQ);
	}
	poClosure(fdata->fd_po);
#ifdef VERBOSE
	printf("partial order (closure):\n");
	poPrint(fdata->fd_po);
#endif


	/* 
	 * do topological sort, and extract new list of filters
	 */
	gTopoSort(&graph);

	/* explicitly create and save a permutation representing the filter order */
	{
		int pos = 0;
		//char buf[BUFSIZ];

		/* XXX -1 since we dont use app */
		fdata->fd_perm = pmNew(fdata->fd_num_filters - 1);

		GLIST(&graph, np) {
			filter_id_t fid;
			filter_info_t *info;

			if(!np->data || np==src_node) {
				continue; /* skip */
			}
			info = (filter_info_t*)np->data;
			fid = info->fi_filterid;
			if(fid == fdata->fd_app_id) {
				continue; /* skip */
			}
			pmSetElt(fdata->fd_perm, pos++, fid);
		}
	}


	/* export filters */
	fprintf(stderr, "filterexec: exporting filter graph to %s.*\n", filename);
	
	{
		node_t *prev = NULL;
		GLIST(&graph, np) {
			edge_t *ep;
			if(prev) {
				ep = gAddEdge(&graph, prev, np);
				ep->eg_color = 1;
			}
			prev = np;
		}
		
	}
	gExport(&graph, filename);


#ifdef VERBOSE
	/* XXX print out the order */
	fprintf(stderr, "filterexec: filter seq ordering");
	for(i=0; i<pmLength(fdata->fd_perm); i++) {
	  fprintf(stderr, " %s", fdata->fd_filters[pmElt(fdata->fd_perm, i)].fi_name);
	}
	fprintf(stderr, "\n");
#endif

	return(0);
}


/*
 * setup things for state-space exploration
 */
static void
initialize_policy(filter_data_t *fdata) {
	opt_policy_t *policy;
	
	/* explicitly create and save a permutation representing the filter order */
	//fdata->fd_perm = fdata_new_perm(fdata);
	/* XXX this is not free'd anywhere */

	/* initialize policy */
	policy = &policy_arr[filter_exec.current_policy];
	assert(policy->policy == filter_exec.current_policy);
	if(policy->p_new) {
	  policy->p_context = policy->p_new(fdata);
	}
	/* XXX this needs to be cleaned up somwehre XXX */
}


/*
 * This initializes all of the filters.  First it parses the filter spec
 * file to create the data structures for each of the filters and the associated
 * state for the filters.  After this we verify each of the filters to make
 * sure we have the minimum required information for the filter.  We then load
 * the shared library that should contain the filter functions and we
 * identify all the entry points of these functions.
 *
 * Inputs:
 * 	lib_name:	The name of the shared library that has the filter files.
 * 	filter_spec:	The file containing the filter spec.
 * 	froot:		A pointer to where the pointer to the list of filters
 * 			should be stored.	
 * Returns:
 * 	0 	if it initailized correctly
 * 	!0	something failed.	
 */
int
fexec_load_searchlet(char *lib_name, char *filter_spec, filter_data_t **fdata)
{
	int			     err;

	log_message(LOGT_FILT, LOGL_TRACE, 
		"init_filters: lib %s spec %s", lib_name, filter_spec);


	fprintf(stderr, "filterexec: reading filter spec %s...\n", filter_spec);
	err = read_filter_spec(filter_spec, fdata);
	if (err) {
		log_message(LOGT_FILT, LOGL_ERR, 
				"Failed to read filter spec <%s>", 
				filter_spec);
		return (err);
	}
	print_filter_list("filterexec: init", *fdata);

	err = resolve_filter_deps(*fdata);
	if (err) {
		log_message(LOGT_FILT, LOGL_ERR, 
				"Failed resolving filter dependancies <%s>", 
				filter_spec);
		return (1);
	}

	err = verify_filters(*fdata);
	if (err) {
		log_message(LOGT_FILT, LOGL_ERR, "Filter verify failed <%s>", 
                filter_spec);
		return (err);
	}

	/* this need to be cleaned up somewhere XXX */
	initialize_policy(*fdata);

	/*
	 * We have loaded the filter spec, now try to load the library
	 * and resolve the dependancies against it.
	 */

    if (lib_name != NULL) {
	    err = load_filter_lib(lib_name, *fdata);
	    if (err) {
		    log_message(LOGT_FILT, LOGL_ERR, 
				    "Failed loading filter library <%s>", 
				    lib_name);
		    return (err);
	    }
    
	    /* everything was loaded correctly, log it */
	    log_message(LOGT_FILT, LOGL_TRACE, 
			    "init_filters: loaded %s", filter_spec);
    }

	return(0);
}

void
update_filter_order(filter_data_t *fdata, const permutation_t *perm) 
{
#if 1 || defined VERBOSE
  char buf[BUFSIZ];
#endif

	pmCopy(fdata->fd_perm, perm);
#if 1|| defined VERBOSE
	printf("changed filter order to: %s\n", pmPrint(perm, buf, BUFSIZ));
#endif

}


/* jump to function (see fexec_opt.c) */
static void
optimize_filter_order(filter_data_t *fdata, opt_policy_t *policy) 
{
	if	(policy->p_optimize) {
    	policy->exploit = policy->p_optimize(policy->p_context, fdata);
  	}
}

static double
tv_diff(struct timeval *end, struct timeval *start)
{
	double	temp;
	long	val;

	if (end->tv_usec > start->tv_usec) {
		val = end->tv_usec - start->tv_usec;
		temp = (double) (end->tv_sec - start->tv_sec);
		temp += val/(double)1000000.0;
	} else {
		val = 1000000 - end->tv_usec - start->tv_usec;
		temp = (double) (end->tv_sec - start->tv_sec - 1);
		temp += val/(double)1000000.0;
	}
	return(temp);
}

double
fexec_get_load(filter_data_t *fdata)
{
	double	temp;

	if ((fdata == NULL) || (fdata->fd_avg_wall == 0)) {
		return(1.0);
	}
#ifdef	XXX	
	temp = fdata->fd_avg_exec/fdata->fd_avg_wall;
	return(temp);
#endif
	return(1.0000000);
}
/*
 * This take an object pointer and a list of filters and evaluates
 * the different filters as appropriate.
 *
 * Input:
 * 	obj_handle:	The object handle for the object to search.
 * 	fdata: 		The data for the filters to evaluate.
 *
 * Return:
 * 	1		Object passed all the filters evaluated.
 * 	0		Object should be dropped. 
 */

int
eval_filters(obj_data_t *obj_handle, filter_data_t *fdata, int force_eval,
         void *cookie,
	     int (*cb_func)(void *cookie, char *name, int *pass, uint64_t* et)) {
	filter_info_t *     cur_filter;
	int			        conf;
	lf_obj_handle_t	    out_list[16];
	char 			    timebuf[BUFSIZ];
	int			        err;
	off_t			    asize;
	int                 pass = 1; /* return value */
	int					rv;
	int cur_fid, cur_fidx;
	static int		loop_cnt = 0;
	struct timeval		wstart;
	struct timeval		wstop;
	struct timezone		tz;
	double			temp;


	/* timer info */
	rtimer_t                rt;	
	u_int64_t               time_ns; /* time for one filter */
	u_int64_t               stack_ns; /* time for whole filter stack */


	log_message(LOGT_FILT, LOGL_TRACE, "eval_filters: Entering");

	fdata->obj_counter++;

	if (fdata->fd_num_filters == 0) {
	  log_message(LOGT_FILT, LOGL_ERR, "eval_filters: no filters");
	  return 1;
	}

	/* change the permutation if it's time for a change */
	optimize_filter_order(fdata, &policy_arr[filter_exec.current_policy]);
    
	if (++loop_cnt > 20) {
		fexec_update_bypass(fdata); 
		loop_cnt = 0; 
	}


	/*
	 * Get the total time we have execute so far (if we have
	 * any) so we can add to the total.
	 */
	/* save the total time info attribute */
	asize = sizeof(stack_ns);
	err = obj_read_attr(&obj_handle->attr_info, FLTRTIME,
		       &asize, (void*)&stack_ns);
	if (err != 0) {
		/* If we didn't find it, then set our count to 0. */
		stack_ns = 0;
	}


	err = gettimeofday(&wstart, &tz);
	assert(err == 0);

	for(cur_fidx = 0; pass && cur_fidx < pmLength(fdata->fd_perm); cur_fidx++) {
	  cur_fid = pmElt(fdata->fd_perm, cur_fidx);
	  cur_filter = &fdata->fd_filters[cur_fid];	

	  active_filter = cur_filter;

	  /*
	   * the name used to store execution time,
	   * we use this to see if this function has already
	   * been executed.
	   */
	  sprintf(timebuf, FLTRTIME_FN, cur_filter->fi_name);

	  asize = sizeof(time_ns);
	  err = obj_read_attr(&obj_handle->attr_info, timebuf, 
			      &asize, (void*)&time_ns);

	  /*
	   * if the read is sucessful, then this stage
	   * has been run.
	   */
	  if (err == 0) {
	    log_message(LOGT_FILT, LOGL_TRACE, 
			"eval_filters: Filter %s has already been run",
			cur_filter->fi_name);
	    continue;
	  }

	  /*
	   * Look at the current filter bypass to see if we should actually
	   * run it or pass it.
	   */
	  if (force_eval == 0) {
	    rv = random();
	    if (rv > cur_filter->fi_bpthresh) {
	      pass = 1;
	      break;
            }
	  }
    
	  cur_filter->fi_called++;

	  /* XXX build the out list appropriately */
	  out_list[0] = obj_handle;

	  /* initialize obj state for this call */
	  obj_handle->cur_offset = 0;
	  obj_handle->cur_blocksize = 1024; /* XXX */


	  /* run the filter and update pass */
	  if(cb_func) {
	    err =  (*cb_func)(cookie, cur_filter->fi_name, &pass, &time_ns);
#define SANITY_NS_PER_FILTER (2 * 1000000000)
	    assert(time_ns < SANITY_NS_PER_FILTER);
	  } else {
	    rt_init(&rt);
	    rt_start(&rt);	/* assume only one thread here */
		  
	    assert(cur_filter->fi_eval_fp);
	    /* arg 3 here looks strange -rw */
	    conf = cur_filter->fi_eval_fp(obj_handle, 1, out_list,
				cur_filter->fi_filt_arg);	

	    /* get timing info and update stats */
	    rt_stop(&rt);
	    time_ns = rt_nanos(&rt);

	    if (conf < cur_filter->fi_threshold) {
	      pass = 0;
	    }
	    log_message(LOGT_FILT, LOGL_TRACE, 
			"eval_filters:  filter %s has val (%d) - threshold %d",
			cur_filter->fi_name, conf, cur_filter->fi_threshold);
	  }

	  cur_filter->fi_time_ns += time_ns; /* update filter stats */

	  stack_ns += time_ns;
	  obj_write_attr(&obj_handle->attr_info, timebuf, 
			 sizeof(time_ns), (void*)&time_ns);

#ifdef PRINT_TIME
	  printf("\t\tmeasured: %f secs\n", rt_time2secs(time_ns));
	  printf("\t\tfilter %s: %f secs cumulative, %f s avg\n",
		 cur_filter->fi_name, rt_time2secs(cur_filter->fi_time_ns),
		 rt_time2secs(cur_filter->fi_time_ns)/cur_filter->fi_called);
#endif

	  if (!pass) {
	    /* XXX cache results if appropriate */
	    cur_filter->fi_drop++;
	  } else {
	    cur_filter->fi_pass++;
	  }

	  fexec_update_prob(fdata, cur_fid, pmArr(fdata->fd_perm), cur_fidx, pass);

	  /* XXX update the time spent on filter */
                
	}
	active_filter = NULL;

	log_message(LOGT_FILT, LOGL_TRACE, 
		    "eval_filters:  done - total time is %lld", stack_ns);

	/* save the total time info attribute */
	obj_write_attr(&obj_handle->attr_info,
		       FLTRTIME,
		       sizeof(stack_ns), (void*)&stack_ns);
	/* track per-object info */
	fstat_add_obj_info(fdata, pass, stack_ns);

	/* update the average time */
	err = gettimeofday(&wstop, &tz);
	assert(err == 0);
	temp = tv_diff(&wstop, &wstart);

	/* XXX debug this better */
	fdata->fd_avg_wall = (0.95 * fdata->fd_avg_wall) + (0.05 * temp);
	temp = rt_time2secs(stack_ns);
	fdata->fd_avg_exec = (0.95 * fdata->fd_avg_exec) + (0.05 * temp);

#if(defined VERBOSE || defined SIM_REPORT)
	{
	  char buf[BUFSIZ];
	  printf("%d average time/obj = %s (%s)\n",
		 fdata->obj_counter,
		 fstat_sprint(buf, fdata),
		 policy_arr[filter_exec.current_policy].exploit ? "EXPLOIT" : "EXPLORE");

	}
#endif

	return pass;
}

