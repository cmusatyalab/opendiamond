
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>
#include <assert.h>
#include <stdint.h>
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

/* #define VERBOSE 1 */

/*
 * Some state to keep track of the active filter. XXX
 */
static filter_info_t  		*active_filter = NULL;
static char			*no_filter = "None";




/* ********************************************************************** */
/* filter optimization policy defs */
/* ********************************************************************** */

typedef struct opt_policy_t {
  void *(*p_new)(const struct filter_data *);
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
// int CURRENT_POLICY = BEST_FIRST_POLICY;


static opt_policy_t policy_arr[] = {
  { NULL, NULL, NULL, NULL },
  { hill_climb_new, hill_climb_delete, hill_climb_optimize, NULL },
  { best_first_new, best_first_delete, best_first_optimize, NULL },
  { indep_new, indep_delete, indep_optimize, NULL },
  { NULL, NULL, NULL, NULL }
};


/*
 * Global state for the filter init code.
 */
int	fexec_fixed_split = 0;	/* we use a fixed partioning if this is 1 */
int	fexec_fixed_ratio = 0;	/* percentage for a fixed partitioning */
int	fexec_cpu_slowdown = 0;	/* percentage slowdown for CPU  */

char 		name_str[40];
char 		pid_str[40];

int
fexec_set_slowdown(void *cookie, int data_len, char *val)
{
	uint32_t	data;
	pid_t		new_pid;
	int			err;
	pid_t		my_pid;
	

	data = *(uint32_t *)val;

	if (fexec_cpu_slowdown != 0) {
		return(EAGAIN);
	}
	
	if (data == 0) {
		return(0);
	}

	if (data > 90) {
		return(EINVAL);
	}


	my_pid = getpid();
	
	new_pid = fork();
	if (new_pid == 0) {
		sprintf(name_str, "%d", data);
		printf("forking with arg %s \n", name_str);
		err = execlp("/home/diamond/bin/slowdown", "slowdown", name_str, 
			pid_str, NULL);
		if (err) {
			
			perror("exec failed:");
		}
	}

	return(0);	

}

/* ********************************************************************** */

void
fexec_system_init() 
{
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
		



static int
load_filter_lib(char *lib_name, filter_data_t *fdata)
{
	void *          handle;
	filter_info_t *	cur_filt;
	filter_proto	fp;
	filter_id_t     fid;
	char *		    error;

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
		fp = dlsym(handle, cur_filt->fi_fname);
		if ((error = dlerror()) != NULL) {
			/* XXX error handling */
			fprintf(stderr, "%s on <%s> \n", error, cur_filt->fi_fname);
			return(ENOENT);
		}
		cur_filt->fi_fp = fp;
#ifdef VERBOSE
		fprintf(stderr, "filter %d (%s): resolved.\n", fid, cur_filt->fi_name);
#endif
		//cur_filt = cur_filt->fi_next;
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
			free(cur_filter->fi_deps[i].name);
			cur_filter->fi_deps[i].name = NULL; /* XXX */
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
			//printf("fid %d = %s\n", fid, info->fi_name);
			if(fid == fdata->fd_app_id) {
				continue; /* skip */
			}
			//printf("fid %d = %s\n", fid, info->fi_name);
			pmSetElt(fdata->fd_perm, pos++, fid);
		}
		//printf("pm: %s\n", pmPrint(fdata->fd_perm, buf, BUFSIZ));
	}


/* #define FILTER_ID_MASK  0x000000FF */
/* #define FILTER_ID(x)    ((filter_id_t)(((uint32_t)(x))&FILTER_ID_MASK)) */
/* 	lastid = INVALID_FILTER_ID; */

/* 	GLIST(&graph, np) { */
/* 		filter_id_t fid; */
		
/* 		fid = FILTER_ID(np->data); */
/* 		if ((np == src_node) || (fid == fdata->fd_app_id)) { */
/* 			continue; /\* skip *\/ */
/* 		} */
/* 		if (lastid == INVALID_FILTER_ID) { */
/* 			fdata->fd_first_filter = fid; */
/* 		} else { */
/* 			fdata->fd_filters[lastid].fi_nextfilter = fid; */
/* 		} */
/* 		lastid = fid; */
/* 	} */
/* 	fdata->fd_filters[lastid].fi_nextfilter = INVALID_FILTER_ID; */
	

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


	/* XXX print out the order */
	printf("filterexec: filter seq ordering");
	for(i=0; i<pmLength(fdata->fd_perm); i++) {
	  printf(" %s", fdata->fd_filters[pmElt(fdata->fd_perm, i)].fi_name);
	}
	printf("\n");

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
init_filters(char *lib_name, char *filter_spec, filter_data_t **fdata)
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
        printf("XXX read fspec  \n");
		return (err);
	}
	print_filter_list("filterexec: init", *fdata);

	err = resolve_filter_deps(*fdata);
	if (err) {
		log_message(LOGT_FILT, LOGL_ERR, 
				"Failed resolving filter dependancies <%s>", 
				filter_spec);
        printf("XXX faile resolve \n");
		return (1);
	}

	err = verify_filters(*fdata);
	if (err) {
        log_message(LOGT_FILT, LOGL_ERR, "Filter verify failed <%s>", 
                filter_spec);
        printf("XXX faile verify \n");
        return (err);
	}

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
update_filter_order(filter_data_t *fdata, const permutation_t *perm) {
#ifdef VERBOSE
  char buf[BUFSIZ];
#endif

	pmCopy(fdata->fd_perm, perm);
#ifdef VERBOSE
	printf("changed filter order to: %s\n", pmPrint(perm, buf, BUFSIZ));
#endif
    /* XXX lh fexec_update_bypass(fdata); */
}


/* jump to function (see fexec_opt.c) */
static void
optimize_filter_order(filter_data_t *fdata, opt_policy_t *policy) {
  if(policy->p_optimize) {
    policy->exploit = policy->p_optimize(policy->p_context, fdata);
  }
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
		} else {
		  rt_init(&rt);
		  rt_start(&rt);	/* assume only one thread here */
		  
		  assert(cur_filter->fi_fp);
		  /* arg 3 here looks strange -rw */
		  conf = cur_filter->fi_fp(obj_handle, 1, out_list, 
					   cur_filter->fi_numargs, cur_filter->fi_args,
					   cur_filter->fi_blob_len, cur_filter->fi_blob_data);

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

#ifdef VERBOSE	
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

