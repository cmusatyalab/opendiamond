
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>
#include <assert.h>
#include <stdint.h>

#include "lib_odisk.h"
#include "lib_log.h"
#include "attr.h"
#include "filter_priv.h"
#include "rtimer.h"
#include "rgraph.h"

// #define VERBOSE 1


/*
 * Prototype for function generated from filter_spec.l
 */
extern int read_filter_spec(char *spec_name, filter_info_t **finfo,
			    char **root_name);


static int
load_filter_lib(char *lib_name, filter_info_t *froot)
{
	void *handle;
	filter_info_t *	cur_filt;
	filter_proto	fp;
	char *		error;

	handle = dlopen(lib_name, RTLD_NOW);
	if (!handle) {
		/* XXX error log */
		fputs(dlerror(), stderr);
		exit (1);
	}

	cur_filt = froot;

	/* XXX keep the handle somewhere */
	while (cur_filt != NULL) {
		fp = dlsym(handle, cur_filt->fi_fname);
		if ((error = dlerror()) != NULL) {
			/* XXX error handling */
			fprintf(stderr, "%s on <%s> \n", error, cur_filt->fi_fname);
			return(ENOENT);
		}
		cur_filt->fi_fp = fp;
		fprintf(stderr, "%s: resolved.\n", cur_filt->fi_name);

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
verify_filters(filter_info_t *froot)
{

	filter_info_t *	cur_filter;

	log_message(LOGT_FILT, LOGL_TRACE, "verify_filters(): starting");


	/*
	 * Make sure we have at least 1 filter defined.
	 */
	if (froot == NULL) {
		log_message(LOGT_FILT, LOGL_ERR, 
				"verify_filters(): no filters");
		return(EINVAL);
	}



	/*
	 * Loop over all the filters and makes sure some minimum requirements
	 * are checked.
	 */
	cur_filter = froot;
	while (cur_filter != NULL) {
		/*
		 * Make sure the filter exisits and it is at least
		 * 1 character.
		 */
		if (strlen(cur_filter->fi_name) == 0) {
			log_message(LOGT_FILT, LOGL_ERR, 
				"verify_filters(): no filter name");
			return(EINVAL);
		}
		if (strlen(cur_filter->fi_fname) == 0) {
			log_message(LOGT_FILT, LOGL_ERR, 
				"verify_filters(): filter name invalid");
			return(EINVAL);
		}

		/*
		 * Make sure the threshold is defined.
		 */
		if (cur_filter->fi_threshold  == -1) {
			log_message(LOGT_FILT, LOGL_ERR, 
				"verify_filters(): no threshold");
			return(EINVAL);
		}
		cur_filter = cur_filter->fi_next;
	}

	log_message(LOGT_FILT, LOGL_TRACE, "verify_filters(): complete");

	return(0);
}


static filter_info_t *
find_filter(filter_info_t *froot, const char *name)
{
	filter_info_t *cur_filter;

	cur_filter = froot;
	while(cur_filter) {
		if(strcmp(cur_filter->fi_name, name) == 0) return cur_filter;
		cur_filter = cur_filter->fi_next;
	}
	return NULL;
}

static void
print_filter_list(char *tag, filter_info_t *froot)
{
	filter_info_t *cur_filter;

	fprintf(stderr, "%s:", tag);
	if(!froot) fprintf(stderr, "<null>");
	cur_filter = froot;
	while(cur_filter) {
		fprintf(stderr, " %s", cur_filter->fi_name);
		cur_filter = cur_filter->fi_next;
	}
	fprintf(stderr, "\n");
}



/* build a label in buf (potential overflow error) using the filter
 * name and attributes. dependency - \\n format used by daVinci. (this
 * should have been pushed into rgraph.c, but it's ok here) */
static void
build_label(char *buf, filter_info_t *fil) {
	int i;

	buf[0] = '\0';
	sprintf(buf, "%s\\n", fil->fi_name);
	for(i=0; i<fil->fi_numargs; i++) {
		strcat(buf, " ");
		strcat(buf, fil->fi_args[i]);
		//strcat(buf, "\\n");
	}
}

/* figure out the filter execution graph from the dependency
 * information. generate the initial ordering.
 * this function also exports the graphs.
 * RETURNS a pointer to the list for the initial order, or NULL if error
 */
static filter_info_t *
resolve_filter_deps(filter_info_t *froot, char *troot_name)
{
	filter_info_t *cur_filter, *tmp, *troot;
	filter_info_t *prev_filter;
	int i;
	graph_t graph;
	node_t *np;
	char *filename = "filters.daVinci";
	char *toponame = "topo.daVinci";
	filter_info_t dummy;
	node_t *src_node;

	/* 
	 * create the graph of filter nodes
	 */
	gInit(&graph);
	src_node = gNewNode(&graph, "DATASRC");
	cur_filter = froot;
	while(cur_filter) {
		char buf[BUFSIZ];
		build_label(buf, cur_filter);
		cur_filter->fi_gnode = gNewNode(&graph, buf);
		cur_filter->fi_gnode->data = cur_filter;
		cur_filter->fi_gnode->val = cur_filter->fi_merit;
		cur_filter = cur_filter->fi_next;
	}

	if(!troot_name) {
		fprintf(stderr, "no root filter found!\n");
		return NULL;
	}
	troot = find_filter(froot, troot_name);
	if(!troot) {
		fprintf(stderr, "could not find root filter <%s>\n", troot_name);
		print_filter_list("list", froot);
		return NULL;
	}
	/* free(troot_name); */	/* XXX */

	/* 
	 * add the dependencies
	 */
	cur_filter = froot;
	while(cur_filter) {
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
			tmp = find_filter(froot, cur_filter->fi_deps[i].name);
			if(!tmp) {
				fprintf(stderr, "could not resolve filter <%s>"
					" needed by <%s>\n",
					cur_filter->fi_deps[i].name,
					cur_filter->fi_name);
				return NULL;
			}
			free(cur_filter->fi_deps[i].name);
			cur_filter->fi_deps[i].name = NULL; /* XX */
			//cur_filter->fi_deps[i].filter = tmp;
			gAddEdge(&graph, tmp->fi_gnode, cur_filter->fi_gnode);
		}
		cur_filter = cur_filter->fi_next;
	}

	/* 
	 * do topological sort, and extract new list of filters
	 */

	gTopoSort(&graph);

	cur_filter = &dummy;
	GLIST(&graph, np) {
		if(np == src_node) continue; /* skip */
		cur_filter->fi_next = (filter_info_t*)np->data;
		cur_filter = (filter_info_t*)np->data;
	}
	cur_filter->fi_next = NULL;
	froot = dummy.fi_next;

	/* export filters */
	fprintf(stderr, "exporting filter graph to %s\n", filename);
	gExport(&graph, filename);

	/* export ordered list */
	{
		graph_t gordered;
		fprintf(stderr, "exporting filter order to %s\n", toponame);
		gInitFromList(&gordered, &graph);
		gExport(&gordered, toponame);
		gClear(&gordered);
	}

	/* ugly hack - delete the application dummy filter from the
	 * end.  unfortunately, we only have a singly-linked list, so
	 * can't get to it easily.  XXX */
	cur_filter = froot;
	prev_filter = NULL;
	while(cur_filter) {
		if(strcmp(cur_filter->fi_name, troot_name) == 0) {
			/* nuke it */
			free(cur_filter);
			cur_filter = NULL;
			if(!prev_filter) {
				fprintf(stderr, "no filters to run!\n");
				return NULL;
			}
			prev_filter->fi_next = NULL;
		} else {
			prev_filter = cur_filter;
			cur_filter = cur_filter->fi_next;
		}	
	}

	/* XXX print out the order */
	print_filter_list("filter topol. ordering", froot);
	return froot;
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
init_filters(char *lib_name, char *filter_spec, filter_info_t **froot)
{
	int			err;
	char	  		*troot;

	log_message(LOGT_FILT, LOGL_TRACE, 
		"init_filters: lib %s spec %s", lib_name, filter_spec);



	err = read_filter_spec(filter_spec, froot, &troot);
	if (err) {
		log_message(LOGT_FILT, LOGL_ERR, 
				"Failed to read filter spec <%s>", 
				filter_spec);
		return (err);
	}
	print_filter_list("read filters", *froot);

	*froot = resolve_filter_deps(*froot, troot);
	if (!*froot) {
		log_message(LOGT_FILT, LOGL_ERR, 
				"Failed resolving filter dependancies <%s>", 
				filter_spec);
		return (1);
	}

	err = verify_filters(*froot);
	if (err) {
		log_message(LOGT_FILT, LOGL_ERR, 
				"Filter verify failed <%s>", 
				filter_spec);
		return (err);
	}


	/*
	 * We have loaded the filter spec, now try to load the library
	 * and resolve the dependancies against it.
	 */

	err = load_filter_lib(lib_name, *froot);
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
 * the different filters as appropriated.
 *
 * Input:
 * 	obj_handle:	The object handle for the object to search.
 * 	froot: 		pointer to the list of filters to evaluate.
 *
 * Return:
 * 	1		Object passed all the filters evaluated.
 * 	0		Object should be dropped. 
 */

int
eval_filters(obj_data_t *obj_handle, filter_info_t *froot)
{
	filter_info_t  		*cur_filter;
	int			conf;
	obj_data_t *		out_list[16];
	char 			buf[BUFSIZ];
	int			err;
	off_t			asize;
	int                     pass = 1; /* return value */

	/* timer info */
	rtimer_t                rt;	
	u_int64_t               time_ns; /* time for one filter */
	u_int64_t               stack_ns; /* time for whole filter stack */


	log_message(LOGT_FILT, LOGL_TRACE, "eval_filters: Entering");

	if (froot == NULL) {
		log_message(LOGT_FILT, LOGL_ERR, 
			"eval_filters: no filter root");
		return 1;
	}

	cur_filter = froot;


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

	while (cur_filter != NULL) {

		/*
		 * the name used to store execution time,
		 * we use this to see if this function has already
		 * been executed.
		 */
		sprintf(buf, FLTRTIME_FN, cur_filter->fi_name);

		asize = sizeof(time_ns);
		err = obj_read_attr(&obj_handle->attr_info, buf, 
				&asize, (void*)&time_ns);

		/*
		 * if the read is sucessful, then this stage
		 * has been run.
		 */
		if (err == 0) {
			log_message(LOGT_FILT, LOGL_TRACE, 
				"eval_filters: Filter %s has already been run",
				cur_filter->fi_name);
			cur_filter = cur_filter->fi_next;
			continue;
		}


		cur_filter->fi_called++;

		/* XXX build the out list appropriately */
		out_list[0] = obj_handle;

		/* initialize obj state for this call */
		obj_handle->cur_offset = 0;
		obj_handle->cur_blocksize = 1024; /* XXX */

		/* XXX save filter name (definite hack) */
		obj_write_attr(&obj_handle->attr_info,
			       "_FILTER.ptr",
			       sizeof(void *), (void*)&cur_filter);

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

		obj_write_attr(&obj_handle->attr_info, buf, 
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
			break;
		}
		cur_filter->fi_pass++;

		/* XXX update the time spent on filter */

		cur_filter = cur_filter->fi_next;
	}
	log_message(LOGT_FILT, LOGL_TRACE, 
	    	"eval_filters:  done - total time is %lld", stack_ns);

	/* save the total time info attribute */
	obj_write_attr(&obj_handle->attr_info,
		       FLTRTIME,
		       sizeof(stack_ns), (void*)&stack_ns);

	return pass;
}

