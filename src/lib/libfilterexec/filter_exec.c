
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
#include "lib_odisk.h"
#include "lib_searchlet.h"
#include "lib_log.h"
#include "attr.h"
#include "filter_exec.h"
#include "filter_priv.h"
#include "rtimer.h"
#include "rgraph.h"

//#define VERBOSE 1

/*
 * Some state to keep track of the active filter.
 */
static filter_info_t  		*active_filter = NULL;
static char			*no_filter = "None";



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
		fprintf(stderr, "%s: resolved.\n", cur_filt->fi_name);
#endif
		cur_filt = cur_filt->fi_next;
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
	int id, tempid, lastid;
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
		cur_filter->fi_gnode->data = (void *)id;
		cur_filter->fi_gnode->val = cur_filter->fi_merit;
	}


	/* 
	 * add the dependencies
	 */
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
		}
	}

	/* 
	 * do topological sort, and extract new list of filters
	 */

	gTopoSort(&graph);

#define FILTER_ID_MASK  0x000000FF
#define FILTER_ID(x)    ((filter_id_t)(((uint32_t)(x))&FILTER_ID_MASK))
	lastid = INVALID_FILTER_ID;

	GLIST(&graph, np) {
        filter_id_t fid;

        fid = FILTER_ID(np->data);
	if ((np == src_node) || (fid == fdata->fd_app_id)) {
                continue; /* skip */
        }
        if (lastid == INVALID_FILTER_ID) {
            fdata->fd_first_filter = fid;
        } else {
            fdata->fd_filters[lastid].fi_nextfilter = fid;
        }
        lastid = fid;
	}
	fdata->fd_filters[lastid].fi_nextfilter = INVALID_FILTER_ID;


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
	print_filter_list("filterexec: filter seq ordering", fdata);
	return(0);
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
	print_filter_list("filterexec", *fdata);

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


	/*
	 * We have loaded the filter spec, now try to load the library
	 * and resolve the dependancies against it.
	 */

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

	return(0);
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
eval_filters(obj_data_t *obj_handle, filter_data_t *fdata)
{
    filter_info_t *     cur_filter;
	int			        conf;
	lf_obj_handle_t	    out_list[16];
	char 			    timebuf[BUFSIZ];
	int			        err;
	off_t			    asize;
	int                 pass = 1; /* return value */
    int                 num_filts;
    filter_id_t *       filt_list;  
    filter_id_t         cur_fid;

	/* timer info */
	rtimer_t                rt;	
	u_int64_t               time_ns; /* time for one filter */
	u_int64_t               stack_ns; /* time for whole filter stack */


	log_message(LOGT_FILT, LOGL_TRACE, "eval_filters: Entering");

	if (fdata->fd_num_filters == 0) {
		log_message(LOGT_FILT, LOGL_ERR, 
			"eval_filters: no filter root");
		return 1;
	}

    filt_list = (filter_id_t *)malloc(sizeof(filter_id_t) *
                    fdata->fd_num_filters);
    assert(filt_list != 0);
    num_filts = 0;

	/*
	 * We need to put more smarts about what filters to evaluate
	 * here as well as the time spent in each of the filters.
	 */ 


	/*
	 * Get the total time we have execute so far (if we have
	 * any) so we can add to the total.
	 */
	/* save the total time info attribute */
	asize = sizeof(stack_ns);
	err = obj_read_attr(&obj_handle->attr_info, FLTRTIME,
		       &asize, (void*)&stack_ns);
	if (err != 0) {
		/* 
		 * If we didn't find it, then set our count to 0.
		 */
		stack_ns = 0;
	}

    cur_fid = fdata->fd_first_filter;
	while ((cur_fid != INVALID_FILTER_ID) && (pass)) {
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
            cur_fid = cur_filter->fi_nextfilter;
			continue;
		}


		cur_filter->fi_called++;

		/* XXX build the out list appropriately */
		out_list[0] = obj_handle;

		/* initialize obj state for this call */
		obj_handle->cur_offset = 0;
		obj_handle->cur_blocksize = 1024; /* XXX */


		rt_start(&rt);	/* assume only one thread here */

		assert(cur_filter->fi_fp);
		/* arg 3 here looks strange -rw */
		conf = cur_filter->fi_fp(obj_handle, 1, out_list, 
				cur_filter->fi_numargs, cur_filter->fi_args);

		/* get timing info and update stats */
		rt_stop(&rt);
		time_ns = rt_nanos(&rt);
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


		log_message(LOGT_FILT, LOGL_TRACE, 
		    	"eval_filters:  filter %s has val (%d) - threshold %d",
			cur_filter->fi_name, conf, cur_filter->fi_threshold);


		if (conf < cur_filter->fi_threshold) {
			/* XXX cache results if appropriate */
			cur_filter->fi_drop++;
			pass = 0;
		} else {
		    cur_filter->fi_pass++;
        }

        fexec_update_prob(fdata, cur_fid, filt_list, num_filts, pass);

		/* XXX update the time spent on filter */

                
        /*
         * Update the list of filters that have been run. for the next
         * run.
         */
        filt_list[num_filts] = cur_fid;
        num_filts++;
    
        /* get the next filter id to run */
        cur_fid = cur_filter->fi_nextfilter;
	}
	active_filter = NULL;
    free(filt_list);

	log_message(LOGT_FILT, LOGL_TRACE, 
	    	"eval_filters:  done - total time is %lld", stack_ns);

	/* save the total time info attribute */
	obj_write_attr(&obj_handle->attr_info,
		       FLTRTIME,
		       sizeof(stack_ns), (void*)&stack_ns);

	return pass;
}

