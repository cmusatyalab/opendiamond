/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 4
 *
 *  Copyright (c) 2002-2005 Intel Corporation
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
 *  Copyright (c) 2008-2009 Carnegie Mellon University
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>
#include <assert.h>
#include <stdint.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>

#include <glib.h>

#include "diamond_consts.h"
#include "diamond_types.h"
#include "lib_tools.h"
#include "dctl_common.h"
#include "dctl_impl.h"
#include "lib_odisk.h"
#include "lib_log.h"
#include "sys_attr.h"
#include "lib_filterexec.h"
#include "filter_priv.h"
#include "fexec_stats.h"
#include "fexec_opt.h"
#include "lib_ocache.h"
#include "dconfig_priv.h"
#include "odisk_priv.h"
#include "ocache_priv.h"
#include "tools_priv.h"
#include "sig_calc_priv.h"


/*
 * #define VERBOSE 1 
 */
/*
 * #define SIM_REPORT 
 */

/*
 * Some state to keep track of the active filter. XXX
 */
filter_info_t  *fexec_active_filter;
static const char    *no_filter = "None";




/*
 * filter optimization policy defs 
 */

enum policy_type_t filter_exec_current_policy = NULL_POLICY;

// int CURRENT_POLICY = NULL_POLICY;
// int CURRENT_POLICY = HILL_CLIMB_POLICY;
// int CURRENT_POLICY = BEST_FIRST_POLICY;

static opt_policy_t policy_arr[] = {
  {
    .policy = NULL_POLICY
  },

  {
    .policy = HILL_CLIMB_POLICY,
    .p_new = hill_climb_new,
    .p_delete = hill_climb_delete,
    .p_optimize = hill_climb_optimize
  },

  {
    .policy = BEST_FIRST_POLICY,
    .p_new = best_first_new,
    .p_delete = best_first_delete,
    .p_optimize = best_first_optimize
  },

  {
    .policy = INDEP_POLICY,
    .p_new = indep_new,
    .p_delete = best_first_delete,
    .p_optimize = best_first_optimize
  },

  {
    .policy = RANDOM_POLICY,
    .p_new = random_new
  },

  {
    .policy = STATIC_POLICY,
    .p_new = static_new
  },

  {
    .policy = NULL_POLICY
  },
};


/*
 * Some filter-runner stuff that doesn't exit on failure.
 */
static void
send_string(FILE *out, const char *str) {
  int len = strlen(str);
  fprintf(out, "%d\n%s\n", len, str);
  fflush(out);
  g_debug("send_string: %d %s", len, str);
}

static void
send_blank(FILE *out) {
  fprintf(out, "\n");
  fflush(out);
  g_debug("send_blank");
}

static void
send_binary(FILE *out, int len, void *data) {
  fprintf(out, "%d\n", len);
  fwrite(data, len, 1, out);
  fprintf(out, "\n");
  fflush(out);
  g_debug("send_binary, len: %d", len);
}

static int
get_size(FILE *in) {
  char *line = NULL;
  size_t n;
  int result;

  if (getline(&line, &n, in) == -1) {
    return -1;
  }

  // if there is no string, then return -1
  if (strlen(line) == 1) {
    result = -1;
  } else {
    result = atoi(line);
  }

  free(line);

  fprintf(stderr, "size: %d\n", result);
  return result;
}

static char
*get_string(FILE *in) {
  int size = get_size(in);

  if (size == -1) {
    return NULL;
  }

  char *result = g_malloc(size + 1);
  result[size] = '\0';

  if (size > 0) {
    if (fread(result, size, 1, in) != 1) {
      return NULL;
    }
  }

  // read trailing '\n'
  getc(in);

  g_debug("get_string: %d %s", size, result);
  return result;
}

static
char **get_strings(FILE *in) {
  GSList *list = NULL;

  char *str;
  while ((str = get_string(in)) != NULL) {
    list = g_slist_prepend(list, str);
  }

  // convert to strv
  int len = g_slist_length(list);
  char **result = g_new(char *, len + 1);
  result[len] = NULL;

  list = g_slist_reverse(list);

  int i = 0;
  while (list != NULL) {
    result[i++] = list->data;
    list = g_slist_delete_link(list, list);
  }

  return result;
}

static void
send_double(FILE *out, double d) {
  char buf[G_ASCII_DTOSTR_BUF_SIZE];
  send_string(out, g_ascii_dtostr (buf, sizeof (buf), d));
}


static char *
get_tag(FILE *in) {
  char *line = NULL;
  size_t n;

  if (getline(&line, &n, in) == -1) {
    return NULL;
  }

  // strip trailing whitespace
  g_strchomp(line);

  // free old
  char *str = g_strdup(line);
  free(line);

  g_debug("get_tag: %s", str);
  return str;
}

static bool
get_int(FILE *in, int *result) {
  char *str = get_string(in);
  if (str == NULL) {
    return false;
  }

  if (sscanf(str, "%d", result) != 1) {
    return false;
  }

  g_debug("get_int: %d", *result);
  return true;
}

static void *
get_binary(FILE *in, int *len_OUT) {
  int size = get_size(in);
  *len_OUT = size;

  uint8_t *binary = NULL;

  if (size > 0) {
    binary = g_malloc(size);

    if (fread(binary, size, 1, in) != 1) {
      *len_OUT = -1;
      return NULL;
    }
  }

  if (size != -1) {
    // read trailing '\n'
    getc(in);
  }

  return binary;
}


/*
 * Global state for the filter init code.
 */
uint32_t	fexec_bypass_type = BP_NONE;
uint32_t	fexec_autopart_type = AUTO_PART_NONE;

void
fexec_system_init(void)
{
	unsigned int    seed;
	int             fd;
	int             rbytes;

	/*
	 * it will default to STD if this doesnt work
	 */
	rtimer_system_init(RTIMER_PAPI);

	dctl_register_u32(DEV_FEXEC_PATH, "split_policy", O_RDWR,
			  &fexec_bypass_type);
	dctl_register_u32(DEV_FEXEC_PATH, "dynamic_method", O_RDWR,
			  &fexec_autopart_type);

	// #ifdef VERBOSE
	fprintf(stderr, "fexec_system_init: policy = %d\n",
		filter_exec_current_policy);
	// #endif


	/*
	 * Initialize the random number generator using a
	 * seed from urandom.  We need to see some randomness
	 * across runs. 
	 */
	fd = open("/dev/urandom", O_RDONLY);
	assert(fd != -1);
	rbytes = read(fd, (void *) &seed, sizeof(seed));
	assert(rbytes == sizeof(seed));

	srandom(seed);
}


const char           *
fexec_cur_filtname(void)
{
	if (fexec_active_filter != NULL) {
		return (fexec_active_filter->fi_name);
	} else {
		return (no_filter);
	}
}

static void
save_blob_data(void *data, size_t dlen, sig_val_t *sig)
{
	char *cache_dir;
	char *name_buf;
	char *sig_str;

	sig_str = sig_string(sig);
	cache_dir = dconf_get_blob_cachedir();
	name_buf = g_strdup_printf(BLOB_FORMAT, cache_dir, sig_str);
	free(sig_str);
	free(cache_dir);

	g_file_set_contents(name_buf, data, dlen, NULL);
	g_free(name_buf);
}


static void
save_filter_state(filter_data_t *fdata, filter_info_t *cur_filt)
{
	char * cache_dir;
	char name_buf[PATH_MAX];
	char *sig_str;
	int i;
	FILE *fp;
	filter_info_t *bfilt;
	filter_id_t fid;
	int num_blobs;

	sig_str = sig_string(&cur_filt->fi_sig);

	cache_dir = dconf_get_filter_cachedir();
	snprintf(name_buf, PATH_MAX, FILTER_CONFIG, cache_dir, sig_str);
	free(sig_str);
	free(cache_dir);

	fp = fopen(name_buf, "w+");
	if (fp == NULL) {
		/* XXX log */
		return;
	}
	fprintf(fp, "FNAME %s\n", cur_filt->fi_name);

	sig_str = sig_string(&fdata->spec_sig);
	fprintf(fp, "SPEC_SIG %s\n", sig_str);
	free(sig_str);

	fprintf(fp, "NUM_OBJECT_FILES %d\n", fdata->num_libs);
	for (i=0; i < fdata->num_libs; i++) {
		sig_str = sig_string(&fdata->lib_info[i].lib_sig);
		fprintf(fp, "OBJECT_FILE %s\n", sig_str);
		free(sig_str);
	}

	num_blobs = 0;
	for (fid = 0; fid < fdata->fd_num_filters; fid++) {
		bfilt = &fdata->fd_filters[fid];
		if (fid == fdata->fd_app_id)
			continue;
		if (bfilt->fi_blob_len > 0)
			num_blobs++;

	}
	fprintf(fp, "NUM_BLOBS %d\n", num_blobs);


	for (fid = 0; fid < fdata->fd_num_filters; fid++) {
		bfilt = &fdata->fd_filters[fid];
		if (fid == fdata->fd_app_id)
			continue;
		if (bfilt->fi_blob_len == 0)
			continue;
	
		fprintf(fp, "BLOBLEN %d\n", bfilt->fi_blob_len);
		sig_str = sig_string(&bfilt->fi_blob_sig);
		fprintf(fp, "BLOBSIG %s\n", sig_str);
		free(sig_str);
		fprintf(fp, "BLOBFILTER %s\n", bfilt->fi_name);
	}

	/* XXX dump other crap */


	fclose(fp);
}


int
fexec_init_search(filter_data_t * fdata)
{
	filter_info_t  *cur_filt;
	filter_id_t     fid;
	int             err;

	/*
	 * clean up the stats 
	 */
	fexec_clear_stats(fdata);

	/*
	 * Go through all the filters and save some information
	 */
	for (fid = 0; fid < fdata->fd_num_filters; fid++) {
		cur_filt = &fdata->fd_filters[fid];
		if (fid == fdata->fd_app_id) {
			continue;
		}

		/*
		 * XXX this need some works 
		 */
		err = digest_cal(fdata,
				 cur_filt->fi_name, cur_filt->fi_eval_name,
				 cur_filt->fi_numargs, cur_filt->fi_arglist,
				 cur_filt->fi_blob_len,
				 cur_filt->fi_blob_data, &cur_filt->fi_sig);

		/* store out some of the cached data */
		if (cur_filt->fi_blob_len > 0) {
			err = sig_cal(cur_filt->fi_blob_data,
				      cur_filt->fi_blob_len,
				      &cur_filt->fi_blob_sig);
			save_blob_data(cur_filt->fi_blob_data,
				       cur_filt->fi_blob_len,
				       &cur_filt->fi_blob_sig);
		}
	}

	/* go through each filter and write out the config to the cache */
	for (fid = 0; fid < fdata->fd_num_filters; fid++) {
		cur_filt = &fdata->fd_filters[fid];
		if (fid == fdata->fd_app_id) {
			continue;
		}
		save_filter_state(fdata, cur_filt);
	}

	return (0);
}



int
fexec_term_search(filter_data_t * fdata) 
{
	filter_info_t  *cur_filt;
	filter_id_t     fid;
	int             bytes;
	int		fd = -1;
	char *		sig_str;
	char		path[PATH_MAX];
	
	if (fdata->full_eval == 0) { 
		char *root = dconf_get_filter_cachedir();
		snprintf(path, PATH_MAX, "%s/results.XXXXXX", root);
		fd = mkstemp(path);
		free(root);
	}	

	/*
	 * Go through all the filters and close its pipes
	 */
	for (fid = 0; fid < fdata->fd_num_filters; fid++) {
		cur_filt = &fdata->fd_filters[fid];
		sig_str = sig_string(&cur_filt->fi_sig);
		log_message(LOGT_FILT, LOGL_INFO, 
				"filter stats: %s (%s), called %d bypassed %d time %lld ns",
				cur_filt->fi_name, sig_str,
				cur_filt->fi_called, cur_filt->fi_bypassed, 
				cur_filt->fi_time_ns);
		log_message(LOGT_FILT, LOGL_INFO, 
				"\tpassed %d dropped %d cache pass %d cache drop %d evals %d",
				cur_filt->fi_pass, cur_filt->fi_drop,
				cur_filt->fi_cache_pass, cur_filt->fi_cache_drop, 
				cur_filt->fi_compute);	
		log_message(LOGT_FILT, LOGL_INFO, 
				"\trefs %d hits %d inter-session %d inter-query %d intra-query %d",
				cur_filt->fi_called, 
				cur_filt->fi_cache_pass + cur_filt->fi_cache_drop,
				cur_filt->fi_hits_inter_session, cur_filt->fi_hits_inter_query, 
				cur_filt->fi_hits_intra_query);	

		free(sig_str);

		if (cur_filt->fi_out_to_runner != NULL) {
			assert(cur_filt->fi_in_from_runner);
			fclose(cur_filt->fi_out_to_runner);
			fclose(cur_filt->fi_in_from_runner);
		}

		if (fd > 0) {
			sig_str = sig_string(&cur_filt->fi_sig);
			bytes = snprintf(path, PATH_MAX, "%s %u %u %u \n",
			    sig_str, fdata->obj_counter, cur_filt->fi_called,
			   cur_filt->fi_drop );
			write(fd, path, bytes); 
			free(sig_str);
		}
	}
	if (fd > 0) 
		close(fd);
	return (0);
}

static void
fail_filter(filter_info_t *cur_filt)
{
	if (cur_filt->fi_out_to_runner != NULL) {
		assert(cur_filt->fi_in_from_runner);
		fclose(cur_filt->fi_out_to_runner);
		fclose(cur_filt->fi_in_from_runner);
	}

	cur_filt->fi_is_initialized = false;
}

static int
load_filter_lib(char *so_name, filter_data_t * fdata,
		sig_val_t * sig)
{
	/*
	 * Store information about this lib.
	 */
	if (fdata->num_libs >= fdata->max_libs) {
		flib_info_t    *new;

		new = realloc(fdata->lib_info, (sizeof(flib_info_t) *
						(fdata->max_libs +
						 FLIB_INCREMENT)));
		assert(new != NULL);

		fdata->lib_info = new;
		fdata->max_libs += FLIB_INCREMENT;
	}

	fdata->lib_info[fdata->num_libs].lib_name = strdup(so_name);
	assert(fdata->lib_info[fdata->num_libs].lib_name != NULL);

	memcpy(&fdata->lib_info[fdata->num_libs].lib_sig, sig, sizeof(*sig));
	fdata->num_libs++;

	return (0);
}

/*
 * This function goes through the filters and makes sure all the
 * required values have been specified.  If we are missing some
 * fields then we return EINVAL.
 */
static int
verify_filters(filter_data_t * fdata)
{

	filter_id_t     fid;

	log_message(LOGT_FILT, LOGL_DEBUG, "verify_filters(): starting");


	/*
	 * Make sure we have at least 1 filter defined.
	 */
	if (fdata->fd_num_filters == 0) {
		log_message(LOGT_FILT, LOGL_ERR,
			    "verify_filters(): no filters");
		printf("num filters is 0 \n");
		return (EINVAL);
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
			return (EINVAL);
		}

		/*
		 * Make sure the threshold is defined.
		 */
		if (fdata->fd_filters[fid].fi_threshold == -1) {
			log_message(LOGT_FILT, LOGL_ERR,
				    "verify_filters(): no threshold");
			printf("bad threshold on %s \n",
			       fdata->fd_filters[fid].fi_name);
			return (EINVAL);
		}
	}

	log_message(LOGT_FILT, LOGL_TRACE, "verify_filters(): complete");

	return (0);
}


static          filter_id_t
find_filter_id(filter_data_t * fdata, const char *name)
{
	filter_id_t     i;

	for (i = 0; i < fdata->fd_num_filters; i++) {
		if (strcmp(fdata->fd_filters[i].fi_name, name) == 0) {
			return (i);
		}
	}
	return (INVALID_FILTER_ID);
}


/*
 * build a label in buf (potential overflow error) using the filter name and
 * attributes. dependency - \\n format used by daVinci. (this should have
 * been pushed into rgraph.c, but it's ok here) 
 */
static void
build_label(char *buf, filter_info_t * fil)
{
	int             i;
	int             width = strlen(fil->fi_name) * 2;

	buf[0] = '\0';
	sprintf(buf, "%s\\n", fil->fi_name);
	for (i = 0; i < fil->fi_numargs; i++) {
		strcat(buf, " ");
		strcat(buf, fil->fi_arglist[i]);
		// strcat(buf, "\\n");
		if (strlen(buf) > width) {	/* too long */
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
resolve_filter_deps(filter_data_t * fdata)
{
	filter_info_t  *cur_filter;
	int             i;
	int             id,
	                tempid;
	// int lastid;
	graph_t         graph;
	node_t         *np;
	node_t         *src_node;

	/*
	 * create the graph of filter nodes
	 */
	gInit(&graph);
	src_node = gNewNode(&graph, "DATASRC");

	for (id = 0; id < fdata->fd_num_filters; id++) {
		char            buf[BUFSIZ];
		cur_filter = &fdata->fd_filters[id];
		build_label(buf, cur_filter);
		cur_filter->fi_gnode = gNewNode(&graph, buf);
		// cur_filter->fi_gnode->data = (void *)id;
		cur_filter->fi_gnode->data = (void *) cur_filter;
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

		/*
		 * everyone depends on source (at least transitively) 
		 */
		if (cur_filter->fi_depcount == 0) {
			gAddEdge(&graph, src_node, cur_filter->fi_gnode);
		}

		/*
		 * add edges; note direction reversed 
		 */
		for (i = 0; i < cur_filter->fi_depcount; i++) {
			tempid =
			    find_filter_id(fdata,
					   cur_filter->fi_deps[i].name);
			if (tempid == INVALID_FILTER_ID) {
				fprintf(stderr,
					"could not resolve filter <%s>"
					" needed by <%s>\n",
					cur_filter->fi_deps[i].name,
					cur_filter->fi_name);
				return (EINVAL);
			}
			// cur_filter->fi_deps[i].filter = tmp;
			gAddEdge(&graph, fdata->fd_filters[tempid].fi_gnode,
				 cur_filter->fi_gnode);

			/*
			 * also build up a partial_order 
			 */
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

	/*
	 * explicitly create and save a permutation representing the filter order 
	 */
	{
		int             pos = 0;
		// char buf[BUFSIZ];

		/*
		 * XXX -1 since we dont use app 
		 */
		fdata->fd_perm = pmNew(fdata->fd_num_filters - 1);

		GLIST(&graph, np) {
			filter_id_t     fid;
			filter_info_t  *info;

			if (!np->data || np == src_node) {
				continue;	/* skip */
			}
			info = (filter_info_t *) np->data;
			fid = info->fi_filterid;
			if (fid == fdata->fd_app_id) {
				continue;	/* skip */
			}
			pmSetElt(fdata->fd_perm, pos++, fid);
		}
	}


	/*
	 * export filters 
	 */
	{
		node_t         *prev = NULL;
		GLIST(&graph, np) {
			edge_t         *ep;
			if (prev) {
				ep = gAddEdge(&graph, prev, np);
				ep->eg_color = 1;
			}
			prev = np;
		}

	}


#ifdef	VERBOSE
	/*
	 * XXX print out the order 
	 */
	fprintf(stderr, "filterexec: filter seq ordering");
	for (i = 0; i < pmLength(fdata->fd_perm); i++) {
		fprintf(stderr, " %s",
			fdata->fd_filters[pmElt(fdata->fd_perm, i)].fi_name);
	}
	fprintf(stderr, "\n");
#endif

	return (0);
}


/*
 * setup things for state-space exploration
 */
static void
initialize_policy(filter_data_t * fdata)
{
	opt_policy_t   *policy;

	/*
	 * explicitly create and save a permutation representing the filter order 
	 */
	// fdata->fd_perm = fdata_new_perm(fdata);
	/*
	 * XXX this is not free'd anywhere 
	 */

	/*
	 * initialize policy 
	 */
	policy = &policy_arr[filter_exec_current_policy];
	assert(policy->policy == filter_exec_current_policy);
	if (policy->p_new) {
		policy->p_context = policy->p_new(fdata);
	}
	/*
	 * XXX this needs to be cleaned up somwehre XXX 
	 */
}


/*
 */
int
fexec_load_obj(filter_data_t * fdata, sig_val_t *sig)
{
	int  err;
	char * cache_dir;
	char so_name[PATH_MAX];
	char *sig_str;

	sig_str = sig_string(sig);
	log_message(LOGT_FILT, LOGL_TRACE, "fexec_load_obj: lib %s", sig_str);

	cache_dir = dconf_get_binary_cachedir();
	snprintf(so_name, PATH_MAX, SO_FORMAT, cache_dir, sig_str);
	free(sig_str);
	free(cache_dir);

	err = load_filter_lib(so_name, fdata, sig);
	if (err) {
		log_message(LOGT_FILT, LOGL_ERR,
			    "Failed loading filter library <%s>",
			    so_name);
		return (err);
	}
	return (0);
}

int
fexec_load_spec(filter_data_t **fdata, sig_val_t *sig)
{
	int err;
	char * cache_dir;
	char name_buf[PATH_MAX];
	char *sig_str;

	sig_str = sig_string(sig);
	log_message(LOGT_FILT, LOGL_TRACE, "fexec_load_spec: spec %s", sig_str);


	cache_dir = dconf_get_spec_cachedir();
	snprintf(name_buf, PATH_MAX, SPEC_FORMAT, cache_dir, sig_str);
	free(sig_str);
	free(cache_dir);

	err = read_filter_spec(name_buf, fdata);
	if (err) {
		log_message(LOGT_FILT, LOGL_ERR,
	    	    "Failed to read filter spec <%s>", name_buf);
		return (err);
	}

	/* save the filter spec signature */
	memcpy(&(*fdata)->spec_sig, sig, sizeof(*sig));

	err = resolve_filter_deps(*fdata);
	if (err) {
		log_message(LOGT_FILT, LOGL_ERR,
		    "Failed resolving filter dependancies <%s>", name_buf);
		return (1);
	}

	err = verify_filters(*fdata);
	if (err) {
		log_message(LOGT_FILT, LOGL_ERR,
			    "Filter verify failed <%s>", name_buf);
		return (err);
	}

	/*
	 * this need to be cleaned up somewhere XXX 
	 */
	initialize_policy(*fdata);
	return(0);
}


void
update_filter_order(filter_data_t * fdata, const permutation_t * perm)
{
#ifdef	VERBOSE
	char            buf[BUFSIZ];
#endif

	pmCopy(fdata->fd_perm, perm);
#ifdef	VERBOSE

	printf("changed filter order to: %s\n", pmPrint(perm, buf, BUFSIZ));
#endif

}


/*
 * jump to function (see fexec_opt.c) 
 */
void
optimize_filter_order(filter_data_t * fdata, opt_policy_t * policy)
{
	if (policy->p_optimize) {
		policy->exploit =
		    policy->p_optimize(policy->p_context, fdata);
	}
}

double
tv_diff(struct timeval *end, struct timeval *start)
{
	double          temp;
	long            val;

	if (end->tv_usec > start->tv_usec) {
		val = end->tv_usec - start->tv_usec;
		temp = (double) (end->tv_sec - start->tv_sec);
		temp += val / (double) 1000000.0;
	} else {
		// val = 1000000 - end->tv_usec - start->tv_usec;
		val = 1000000 + end->tv_usec - start->tv_usec;
		temp = (double) (end->tv_sec - start->tv_sec - 1);
		temp += val / (double) 1000000.0;
	}
	return (temp);
}

double
fexec_get_load(filter_data_t * fdata)
{
#ifdef	XXX
	double          temp;
#endif

	if ((fdata == NULL) || (fdata->fd_avg_wall == 0)) {
		return (1.0);
	}
#ifdef	XXX
	temp = fdata->fd_avg_exec / fdata->fd_avg_wall;
	return (temp);
#endif

	return (1.0000000);
}

static bool
streq(const char *a, const char *b)
{
	return strcmp(a, b) == 0;
}

int
run_eval_server(FILE *in, FILE *out, obj_data_t *obj_handle, filter_info_t *cur_filt)
{
  while (true) {
    // read tag
    char *tag = get_tag(in);
    if (tag == NULL) {
      fail_filter(cur_filt);
      return 0;
    }

    // switch
    if (streq(tag, "get-attribute")) {
      // read name
      char *name = get_string(in);
      if (name == NULL) {
	fail_filter(cur_filt);
	return 0;
      }

      // look up attribute
      unsigned int len;
      unsigned char *data;
      int result = lf_internal_ref_attr(obj_handle, name, &len, &data);
      g_free(name);

      // write attribute
      if (result != 0) {
	// no attribute
	send_blank(out);
      } else {
	send_binary(out, len, data);
      }
    } else if (streq(tag, "set-attribute")) {
      // read name
      char *name = get_string(in);
      if (name == NULL) {
	fail_filter(cur_filt);
	return 0;
      }

      // read value
      int len;
      void *data = get_binary(in, &len);
      if (len == -1) {
	fail_filter(cur_filt);
	return 0;
      }

      lf_internal_write_attr(obj_handle, name, len, data);
      g_free(name);
      g_free(data);
    } else if (streq(tag, "omit-attribute")) {
      // read name
      char *name = get_string(in);
      if (name == NULL) {
	fail_filter(cur_filt);
	return 0;
      }

      int result = lf_internal_omit_attr(obj_handle, name);
      g_free(name);

      // write result
      if (result == 0) {
	send_string(out, "true");
      } else {
	send_string(out, "false");
      }
    } else if (streq(tag, "get-session-variables")) {
      // get list
      char **names = get_strings(in);

      // populate result
      int count = g_strv_length(names);
      double *results = g_new0(double, count);
      lf_internal_get_session_variables(obj_handle, names, results);

      g_strfreev(names);

      // output result
      for (int i = 0; i < count; i++) {
	send_double(out, results[i]);
      }
      send_blank(out);

      g_free(results);
    } else if (streq(tag, "update-session-variables")) {
      // TODO

    } else if (streq(tag, "log")) {
      // read level
      int level;
      if (!get_int(in, &level)) {
	fail_filter(cur_filt);
	return 0;
      }

      // read message
      char *msg = get_string(in);
      if (msg == NULL) {
	fail_filter(cur_filt);
	return 0;
      }

      // log it
      log_message(LOGT_APP, level, "%s", msg);
      g_free(msg);
    } else if (streq(tag, "stdout")) {
      // read message
      char *msg = get_string(in);
      if (msg == NULL) {
	fail_filter(cur_filt);
	return 0;
      }

      // print it
      printf("%s", msg);
      g_free(msg);
    } else if (streq(tag, "result")) {
      int result;
      if (get_int(in, &result)) {
	return result;
      } else {
	fail_filter(cur_filt);
	return 0;
      }
    } else {
      fail_filter(cur_filt);
      return 0;
    }
  }
}

/*
 * This take an object pointer and a list of filters and evaluates
 * the different filters as appropriate.
 *
 * Input:
 *  obj_handle: The object handle for the object to search.
 *  fdata:      The data for the filters to evaluate.
 *
 * Return:
 *  1       Object passed all the filters evaluated.
 *  0       Object should be dropped. 
 */

int
eval_filters(obj_data_t * obj_handle, filter_data_t * fdata, int force_eval,
	     double *elapsed,
	     void *cookie, int (*continue_cb) (void *cookie),
	     int (*cb_func) (void *cookie, char *name,
			     int *pass, uint64_t * et))
{
	filter_info_t  *cur_filter;
	int             val;
	char            timebuf[BUFSIZ];
	int             err;
	size_t          asize;
	int             pass = 1;	/* return value */
	int             rv;
	int             cur_fid,
	                cur_fidx;
	struct timeval  wstart;
	struct timeval  wstop;
	struct timezone tz;
	double          temp;

	char           *sig_str;

	/*
	 * timer info 
	 */
	rtimer_t        rt;
	u_int64_t       time_ns;	/* time for one filter */
	u_int64_t       stack_ns = 0;	/* time for whole filter stack */

	sig_str = sig_string(&obj_handle->id_sig);
	log_message(LOGT_FILT, LOGL_TRACE, "eval_filters(%s): Entering",
		    sig_str);
	free(sig_str);

	fdata->obj_counter++;

	if (fdata->fd_num_filters == 0) {
		log_message(LOGT_FILT, LOGL_ERR, "eval_filters: no filters");
		return 1;
	}

	/*
	 * change the permutation if it's time for a change 
	 */
	optimize_filter_order(fdata, &policy_arr[filter_exec_current_policy]);

	/*
	 * Get the total time we have execute so far (if we have
	 * any) so we can add to the total.
	 */
	/*
	 * save the total time info attribute 
	 */
	asize = sizeof(stack_ns);
	obj_read_attr(&obj_handle->attr_info, FLTRTIME,
		      &asize, (void *) &stack_ns);

	err = gettimeofday(&wstart, &tz);
	assert(err == 0);

	for (cur_fidx = 0; pass && cur_fidx < pmLength(fdata->fd_perm);
	     cur_fidx++) {
		cur_fid = pmElt(fdata->fd_perm, cur_fidx);
		cur_filter = &fdata->fd_filters[cur_fid];

		fexec_active_filter = cur_filter;

		/*
		 * the name used to store execution time,
		 * we use this to see if this function has already
		 * been executed.
		 */
		sprintf(timebuf, FLTRTIME_FN, cur_filter->fi_name);

		asize = sizeof(time_ns);
		err = obj_read_attr(&obj_handle->attr_info, timebuf,
				    &asize, (void *) &time_ns);

		/*
		 * if the read is sucessful, then this stage
		 * has been run.
		 */
		if (err == 0) {
			sig_str = sig_string(&obj_handle->id_sig);
			log_message(LOGT_FILT, LOGL_TRACE,
						"eval_filters(%s): Filter %s has already been run",
						sig_str,
						cur_filter->fi_name);
			free(sig_str);
			continue;
		}

		/*
		 * Look at the current filter bypass to see if we should actually
		 * run it or pass it.  For the non-auto partitioning, we
		 * we still use the bypass values to determine how much of
		 * the allocation to run.
		 */
		if (force_eval == 0) {
			if ((fexec_autopart_type == AUTO_PART_BYPASS) ||
			    (fexec_autopart_type == AUTO_PART_NONE)) {
				rv = random();
				if (rv > cur_filter->fi_bpthresh) {
					pass = 1;
					break;
				}
			} else if ((fexec_autopart_type == AUTO_PART_QUEUE) &&
				   (cur_filter->fi_firstgroup)) {
				if ((*continue_cb) (cookie) == 0) {
					pass = 1;
					break;
				}
			}
		}

		cur_filter->fi_called++;


		/*
		 * run the filter and update pass 
		 */
		if (cb_func) {
			err =
			    (*cb_func) (cookie, cur_filter->fi_name, &pass,
					&time_ns);
#define SANITY_NS_PER_FILTER (2 * 1000000000)

			assert(time_ns < SANITY_NS_PER_FILTER);
		} else {
			/* do lazy initialization if necessary */
			fexec_possibly_init_filter(cur_filter,
						   fdata->num_libs,
						   fdata->lib_info,
						   fdata->fd_num_filters,
						   fdata->fd_filters,
						   fdata->fd_app_id);

			rt_init(&rt);
			rt_start(&rt);	/* assume only one thread here */

			val = run_eval_server(cur_filter->fi_in_from_runner,
					      cur_filter->fi_out_to_runner,
					      obj_handle,
					      cur_filter);

			/*
			 * get timing info and update stats 
			 */
			rt_stop(&rt);
			time_ns = rt_nanos(&rt);

			if (val == -1) {
				cur_filter->fi_error++;
				pass = 0;
			} else if (val < cur_filter->fi_threshold) {
				pass = 0;
			}

			char *sig_str1 = sig_string(&obj_handle->id_sig);
			char *sig_str2 = sig_string(&cur_filter->fi_sig);
			log_message(LOGT_FILT, LOGL_TRACE, 
					"eval_filters(%s): filter %s (%s) %s, time %lld ns",  
					sig_str1,
					cur_filter->fi_name, 
					sig_str2,
					pass?"PASS":"FAIL",
					time_ns);
			free(sig_str1);
			free(sig_str2);
		}

		cur_filter->fi_time_ns += time_ns;	/* update filter
							 * stats */

		stack_ns += time_ns;
		obj_write_attr(&obj_handle->attr_info, timebuf,
			       sizeof(time_ns), (void *) &time_ns);
		if (!pass) {
			cur_filter->fi_drop++;
		} else {
			cur_filter->fi_pass++;
		}

		fexec_update_prob(fdata, cur_fid, pmArr(fdata->fd_perm),
				  cur_fidx, pass);

		/*
		 * XXX update the time spent on filter 
		 */

	}

	if ((cur_fidx >= pmLength(fdata->fd_perm)) && pass) {
		pass = 2;
	}

	fexec_active_filter = NULL;

	sig_str = sig_string(&obj_handle->id_sig);
	log_message(LOGT_FILT, LOGL_TRACE, 
				"eval_filters(%s): %s total time %lld ns",
				sig_str,
				pass?"PASS":"FAIL",
				stack_ns);
	free(sig_str);

	/*
	 * save the total time info attribute 
	 */
	obj_write_attr(&obj_handle->attr_info,
		       FLTRTIME, sizeof(stack_ns), (void *) &stack_ns);
	/*
	 * track per-object info 
	 */
	fstat_add_obj_info(fdata, pass, stack_ns);

	/*
	 * update the average time 
	 */
	err = gettimeofday(&wstop, &tz);
	assert(err == 0);
	temp = tv_diff(&wstop, &wstart);

	/*
	 * XXX debug this better 
	 */
	fdata->fd_avg_wall = (0.95 * fdata->fd_avg_wall) + (0.05 * temp);
	temp = rt_time2secs(stack_ns);
	*elapsed = temp;
	fdata->fd_avg_exec = (0.95 * fdata->fd_avg_exec) + (0.05 * temp);

#if(defined VERBOSE || defined SIM_REPORT)
	{
		char            buf[BUFSIZ];
		printf("%d average time/obj = %s (%s)\n",
		       fdata->obj_counter,
		       fstat_sprint(buf, fdata),
		       policy_arr[filter_exec.current_policy].
		       exploit ? "EXPLOIT" : "EXPLORE");

	}
#endif

	return pass;
}

void
fexec_possibly_init_filter(filter_info_t *cur_filt,
			   int num_libs, flib_info_t *flibs,
			   int fd_num_filters, filter_info_t *fd_filters,
			   filter_id_t fd_app_id)
{
	int i;

	if (cur_filt->fi_is_initialized) {
		return;
	}

	// probe all libraries for our functions
	for (i = 0; i < num_libs; i++) {
		flib_info_t *fl = flibs + i;
		char *so_name = fl->lib_name;

		// launch runner
		char *argv[] = { FILTER_RUNNER_PATH, NULL };
		int runner_input;
		int runner_output;
		if (!g_spawn_async_with_pipes (NULL, argv, NULL, 0, NULL, NULL, NULL,
					       &runner_input,
					       &runner_output,
					       NULL, NULL)) {
			// it is really bad if we can't even start
			abort();
		}

		cur_filt->fi_out_to_runner = fdopen(runner_input, "w");
		if (!cur_filt->fi_out_to_runner) {
			perror("Can't make file from runner's stdin");
			abort();
		}
		cur_filt->fi_in_from_runner = fdopen(runner_output, "r");
		if (!cur_filt->fi_in_from_runner) {
			perror("Can't make file from runner's stdout");
			abort();
		}


		// feed it arguments

		// soname
		send_string(cur_filt->fi_out_to_runner,
			    so_name);
		// init, eval, fini
		send_string(cur_filt->fi_out_to_runner,
			    cur_filt->fi_init_name);
		send_string(cur_filt->fi_out_to_runner,
			    cur_filt->fi_eval_name);
		send_string(cur_filt->fi_out_to_runner,
			    cur_filt->fi_fini_name);
		// args
		for (int i2 = 0; i2 < cur_filt->fi_numargs; i2++) {
		  send_string(cur_filt->fi_out_to_runner,
			      cur_filt->fi_arglist[i2]);
		}
		send_blank(cur_filt->fi_out_to_runner);
		// blob
		send_binary(cur_filt->fi_out_to_runner,
			    cur_filt->fi_blob_len,
			    cur_filt->fi_blob_data);
		// name
		send_string(cur_filt->fi_out_to_runner,
			    cur_filt->fi_name);

		// read out the string
		char *result = get_tag(cur_filt->fi_in_from_runner);
		if (!result || (strcmp(result, "symbols-resolved") != 0)) {
		  // incorrect so for these symbols, try again
		  g_free(result);
		  fclose(cur_filt->fi_in_from_runner);
		  fclose(cur_filt->fi_out_to_runner);

		  cur_filt->fi_in_from_runner = NULL;
		  cur_filt->fi_out_to_runner = NULL;

		  continue;
		}
		g_free(result);

		// we found it
		break;
	}

	cur_filt->fi_is_initialized = true;

	// check the files
	g_return_if_fail (cur_filt->fi_in_from_runner &&
			  cur_filt->fi_out_to_runner);
}
