/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 3
 *
 *  Copyright (c) 2002-2005 Intel Corporation
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
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
#include <dlfcn.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>
#include <assert.h>
#include <stdint.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>

#include "diamond_consts.h"
#include "diamond_types.h"
#include "lib_tools.h"
#include "lib_dctl.h"
#include "dctl_common.h"
#include "lib_odisk.h"
#include "lib_log.h"
#include "sys_attr.h"
#include "lib_filterexec.h"
#include "filter_priv.h"
#include "fexec_stats.h"
#include "fexec_opt.h"
#include "lib_ocache.h"
#include "lib_dconfig.h"
#include "ocache_priv.h"


#define	MAX_PERM_NUM	5

static void     sample_init(void);
static int      dynamic_use_oattr(void);
static void     oattr_sample(void);

static permutation_t *cached_perm[MAX_PERM_NUM];
static int      cached_perm_num = 0;
static int      perm_done = 0;

static int      search_active = 0;
static int      ceval_blocked = 0;
static pthread_mutex_t ceval_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t active_cv = PTHREAD_COND_INITIALIZER;	/* active */

static opt_policy_t policy_arr[] = {
	{NULL_POLICY, NULL, NULL, NULL, NULL},
	{HILL_CLIMB_POLICY, hill_climb_new, hill_climb_delete,
	 hill_climb_optimize, NULL},
	{BEST_FIRST_POLICY, best_first_new, best_first_delete,
	 best_first_optimize, NULL},
	{INDEP_POLICY, indep_new, best_first_delete,
	 best_first_optimize, NULL},
	{RANDOM_POLICY, random_new, NULL, NULL, NULL},
	{STATIC_POLICY, static_new, NULL, NULL, NULL},
	{NULL_POLICY, NULL, NULL, NULL, NULL}
};

static unsigned int    use_cache_table = 1;
static unsigned int    use_cache_oattr = 1;
static unsigned int    cache_oattr_thresh = 10000;
static unsigned int    mdynamic_load = 1;
static unsigned int	add_cache_entries = 1;
static unsigned int	hybrid_mode_enabled = 1;

/*
 * data structure to keep track of the good names we need to process
 * due to optimistic evaluation.
 */
typedef struct inject_names {
	char **nlist;
	int	num_name;
	int	cur_name;
	TAILQ_ENTRY(inject_names)	in_link;
} inject_names_t;

static TAILQ_HEAD(, inject_names) inject_list = 
    TAILQ_HEAD_INITIALIZER(inject_list);


static void
mark_end(void)
{
	pr_obj_t       *pr_obj;

	pr_obj = (pr_obj_t *) malloc(sizeof(*pr_obj));
	assert(pr_obj != NULL);
	pr_obj->obj_id = 0;
	pr_obj->obj_name = NULL;
	pr_obj->oattr_fnum = -1;
	pr_obj->stack_ns = 0;
	odisk_pr_add(pr_obj);
}



static void    *
ceval_main(void *arg)
{
	ceval_state_t  *cstate = (ceval_state_t *) arg;
	char           *new_name;
	inject_names_t *names; 
	int             err;

	while (1) {
		pthread_mutex_lock(&ceval_mutex);
		while (search_active == 0) {
			ceval_blocked = 1;
			err = pthread_cond_wait(&active_cv, &ceval_mutex);
		}
		ceval_blocked = 0;
		pthread_mutex_unlock(&ceval_mutex);

		/*
		 * If there are names in the 'good' list we process there
		 * names first. otherwise we get a file name from odisk.
		 */	
		if ((names = TAILQ_FIRST(&inject_list)) != NULL) {		
			if (names->num_name == names->cur_name) {
				free(names->nlist);
				TAILQ_REMOVE(&inject_list, names, in_link);
				free(names);
				continue;
			}
			new_name = names->nlist[names->cur_name++];
		} else {
			new_name = odisk_next_obj_name(cstate->odisk);
		}

		if (new_name != NULL) {
			ceval_filters1(new_name, cstate->fdata, cstate);
		} else {
			mark_end();
			search_active = 0;
		}
	}
}

/*
 * We have recieved a list of good names that have been partially
 * processed.  We need to add these to the list of items we will search
 * again.
 */

void
ceval_inject_names(char **nl, int nents)
{
	inject_names_t *names;

	/* allocate structure and put on the list of items */
	names = malloc(sizeof(*names));
	names->nlist = nl;
	names->num_name = nents;
	names->cur_name = 0;
	TAILQ_INSERT_TAIL(&inject_list, names, in_link);

	/*
	 * we need to tell libodisk there is more data in case
	 * we had previously told it we were done.
	 */ 
	odisk_continue();

	/*
	 * Wake up current thread that may have thought we were done
	 * processing.
	 */
	pthread_mutex_lock(&ceval_mutex);
	search_active = 1;
	pthread_cond_signal(&active_cv);
	pthread_mutex_unlock(&ceval_mutex);
}

/*
 *  Clean up the list of good names from an older search by walking
 *  the list and free'ing any resources.
 */
static void
ceval_reset_inject(void)
{
	inject_names_t *names;
	int	i;

	while ((names = TAILQ_FIRST(&inject_list)) != NULL) {		
		for (i=names->cur_name; i < names->num_name; i++) {
			free(names->nlist[i]);
		}
		free(names->nlist);
		TAILQ_REMOVE(&inject_list, names, in_link);
		free(names);
	}
}


/*
 * called when DEV_SEARCHLET 
 */
int
ceval_init_search(filter_data_t * fdata, query_info_t *qinfo, ceval_state_t * cstate)
{
	filter_id_t     fid;
	filter_info_t  *cur_filt;
	int             err;
	char            buf[PATH_MAX];
	char           *cdir;
	char           *sig_str;
	struct timeval  atime;
	struct timezone tz;

	err = gettimeofday(&atime, &tz);
	assert(err == 0);

	ceval_reset_inject();

	pthread_mutex_lock(&ceval_mutex);
	for (fid = 0; fid < fdata->fd_num_filters; fid++) {
		cur_filt = &fdata->fd_filters[fid];
		if (fid == fdata->fd_app_id) {
			continue;
		}
		err = digest_cal(fdata, cur_filt->fi_eval_name,
			 cur_filt->fi_numargs, cur_filt->fi_arglist, 
			 cur_filt->fi_blob_len, cur_filt->fi_blob_data, 
			 &cur_filt->fi_sig);

		sig_str = sig_string(&cur_filt->fi_sig);
		if (sig_str == NULL) {
			continue;
		}
		cdir = dconf_get_cachedir();
		sprintf(buf, "%s/%s", cdir, sig_str);
		free(sig_str);
		err = mkdir(buf, 0777);
		if (err && errno != EEXIST) {
			printf("fail to creat dir %s, err %d\n", buf, err);
		}
		ocache_read_file(cdir, &cur_filt->fi_sig,
				 &cur_filt->cache_table, &atime);
		free(cdir);
	}

	cstate->fdata = fdata;
	cstate->qinfo = qinfo;
	pthread_mutex_unlock(&ceval_mutex);

	return (0);
}

/*
 * called when there is a new connection 
 */
int
ceval_init(ceval_state_t ** cstate, odisk_state_t * odisk, void *cookie,
	   stats_drop stats_drop_fn, stats_process stats_process_fn)
{
	int             err;
	ceval_state_t  *new_state;

	new_state = (ceval_state_t *) calloc(1, sizeof(*new_state));
	assert(new_state != NULL);
	dctl_register_leaf(DEV_CACHE_PATH, "use_cache_table", DCTL_DT_UINT32,
			   dctl_read_uint32, dctl_write_uint32,
			   &use_cache_table);
	dctl_register_leaf(DEV_CACHE_PATH, "use_cache_oattr", DCTL_DT_UINT32,
			   dctl_read_uint32, dctl_write_uint32,
			   &use_cache_oattr);
	dctl_register_leaf(DEV_CACHE_PATH, "cache_oattr_thresh",
			   DCTL_DT_UINT32, dctl_read_uint32,
			   dctl_write_uint32, &cache_oattr_thresh);
	dctl_register_leaf(DEV_CACHE_PATH, "mdynamic_load", DCTL_DT_UINT32,
			   dctl_read_uint32, dctl_write_uint32,
			   &mdynamic_load);
	dctl_register_leaf(DEV_CACHE_PATH, "add_cache_entries", DCTL_DT_UINT32,
			   dctl_read_uint32, dctl_write_uint32,
			   &add_cache_entries);
	dctl_register_leaf(DEV_CACHE_PATH, "hybrid_mode_enabled", DCTL_DT_UINT32,
			   dctl_read_uint32, dctl_write_uint32,
			   &hybrid_mode_enabled);

	new_state->odisk = odisk;
	new_state->cookie = cookie;
	new_state->stats_drop_fn = stats_drop_fn;
	new_state->stats_process_fn = stats_process_fn;

	err = pthread_create(&new_state->ceval_thread_id, NULL,
			     ceval_main, (void *) new_state);

	*cstate = new_state;

	return (0);
}

int
ceval_start(filter_data_t * fdata)
{
	int             i;
	char            buf[BUFSIZ];

	pthread_mutex_lock(&ceval_mutex);
	perm_done = 0;
	for (i = 0; i < MAX_PERM_NUM; i++) {
		cached_perm[i] = NULL;
	}
	cached_perm[0] = fdata->fd_perm;
	cached_perm_num = 1;
	pmPrint(fdata->fd_perm, buf, BUFSIZ);
	search_active = 1;
	sample_init();
	pthread_cond_signal(&active_cv);
	pthread_mutex_unlock(&ceval_mutex);

	return (0);
}

int
ceval_stop(filter_data_t * fdata)
{
	filter_id_t     fid;
	filter_info_t  *cur_filt;
	int             i;
	struct timespec ts;

	/*
	 * set search inactive and wait background thread to block. 
	 */
	search_active = 0;
	while (ceval_blocked == 0) {
		ts.tv_sec = 0;
		ts.tv_nsec = 10000000;	/* 10 ms */
		nanosleep(&ts, NULL);
	}

	pthread_mutex_lock(&ceval_mutex);
	for (i = 0; i < cached_perm_num; i++) {
		pmDelete(cached_perm[i]);
		cached_perm[i] = NULL;
	}
	cached_perm_num = 0;

	for (fid = 0; fid < fdata->fd_num_filters; fid++) {
		cur_filt = &fdata->fd_filters[fid];
		if (fid == fdata->fd_app_id) {
			continue;
		}
		ocache_stop_search(&cur_filt->fi_sig);
	}

	pthread_mutex_unlock(&ceval_mutex);

	return (0);
}

/*
 * XXX need a smarter algorithm later 
 */
static int
generate_new_perm(const partial_order_t * po, permutation_t * copy, int fidx,
		  permutation_t ** new_perm)
{
	permutation_t  *ptr;
	int             i,
	                j;
	int            *list1,
	               *list2;
	int             list1_num,
	                list2_num;
	int             index;

	if (copy == NULL)
		return (EINVAL);
	ptr = pmDup(copy);
	if (ptr == NULL)
		return (ENOMEM);

	list1 = (int *) malloc(sizeof(int) * pmLength(copy));
	assert(list1 != NULL);
	list2 = (int *) malloc(sizeof(int) * pmLength(copy));
	assert(list2 != NULL);
	list1[0] = fidx;
	list1_num = 1;
	list2_num = 0;

	for (i = fidx + 1; i < pmLength(copy); i++) {
		if (poGet(po, pmElt(copy, i), pmElt(copy, fidx)) == PO_GT) {
			list1[list1_num] = i;
			list1_num++;
		} else {
			list2[list2_num] = i;
			list2_num++;
		}
	}
	for (i = 0, j = fidx; i < list2_num; i++, j++) {
		index = list2[i];
		ptr->elements[j] = copy->elements[index];
	}
	for (i = 0; i < list1_num; i++, j++) {
		index = list1[i];
		ptr->elements[j] = copy->elements[index];
	}

	*new_perm = ptr;
	free(list1);
	free(list2);

	return (0);
}

static
void source_cache_hit(filter_info_t *f, sig_val_t *oid_sig,
					  cache_attr_set *change_attr,
					  query_info_t *qinfo, query_info_t *einfo) 
{
	int found = 1;
	int conf;
	cache_attr_set *oattr_set;
	sig_val_t isig;
	query_info_t entry_info;
	
	if (einfo == NULL) {
		// look up the cache entry
		found = cache_lookup(oid_sig, &f->fi_sig,
					     	f->cache_table,
					     	change_attr, &conf, &oattr_set,
					     	&isig, &entry_info);
	} else {
		entry_info = *einfo;
	}

	if (found) {
		if (memcmp(qinfo, &entry_info, sizeof(query_info_t)) == 0) {
			f->fi_hits_intra_query++;
		} else if (memcmp(&qinfo->session, &entry_info.session, 
					  		sizeof(session_info_t)) == 0) {
			f->fi_hits_inter_query++;
		} else {
			f->fi_hits_inter_session++;
		}
	}
	
	return;
}

int
ceval_filters1(char *objname, filter_data_t * fdata, void *cookie)
{
	ceval_state_t  *cstate = (ceval_state_t *) cookie;
	filter_info_t  *cur_filter;
	int             conf;
	int             err;
	int             found;
	int             pass = 1;	/* return value */
	int             cur_fid,
	                cur_fidx = 0;
	struct timeval  wstart;
	struct timeval  wstop;
	struct timezone tz;
	double          temp;
	rtimer_t        rt;
	u_int64_t       time_ns;	/* time for one filter */
	u_int64_t       stack_ns;	/* time for whole filter stack */
	cache_attr_set  change_attr;
	cache_attr_set *oattr_set;
	int             hit = 1;
	int             oattr_fnum = 0;
	sig_val_t       isig;
	sig_val_t       id_sig;
	pr_obj_t       *pr_obj;
	int             i,
	                j;
	int             perm_num;
	permutation_t  *cur_perm, *new_perm = NULL;
	char            buf[BUFSIZ];
	query_info_t    entry_info;

	/*
	 * XXX this used to be passed in and need to change before caching is 
	 * re-enabled. 
	 */

	sig_cal_str(objname, &id_sig);

	fdata->obj_counter++;

	/*
	 * we are going to needs this in most cases, so allocate now 
	 */
	pr_obj = (pr_obj_t *) malloc(sizeof(*pr_obj));
	assert(pr_obj != NULL);



	if (use_cache_table == 0) {
		pr_obj->obj_name = objname;
		pr_obj->oattr_fnum = 0;
		pr_obj->stack_ns = 0;
		odisk_pr_add(pr_obj);
		return (1);
	}

	if (fdata->fd_num_filters == 0) {
		log_message(LOGT_FILT, LOGL_ERR, "ceval_filters1: no filters");
		return 1;
	}


	stack_ns = 0;

	err = gettimeofday(&wstart, &tz);
	assert(err == 0);

	for (perm_num = 0; perm_num < cached_perm_num; perm_num++) {
		change_attr.entry_num = 0;
		change_attr.entry_data = calloc(1,
						ATTR_ENTRY_NUM *
						sizeof(char *));
		assert(change_attr.entry_data != NULL);

		/*
		 * get initial attributes of object from cache.
		 * if we don't find object, there isn't any point.
		 */
		found = cache_get_init_attrs(&id_sig, &change_attr);
		if (!found) {
			free(change_attr.entry_data);
			hit = 0;
			break;
		}

		cur_perm = cached_perm[perm_num];
		assert(cur_perm != NULL);
		hit = 1;
		/*
		 * here we do cache lookup based on object_id, filter 
		 * signature and changed_attr_set. stop when we get a cache 
		 * miss or object drop 
		 */
		for (cur_fidx = 0; pass && cur_fidx < pmLength(cur_perm);
		     cur_fidx++) {
			cur_fid = pmElt(cur_perm, cur_fidx);
			cur_filter = &fdata->fd_filters[cur_fid];

			rt_init(&rt);
			rt_start(&rt);

			found = cache_lookup(&id_sig, &cur_filter->fi_sig,
					     cur_filter->cache_table,
					     &change_attr, &conf, &oattr_set,
					     &isig, &entry_info);

			if (found) {
				/*
				 * a hit!  a very palpable hit!
				 * get the cached output attr set and 
				 * modify changed attr set 
				 */
				if (conf < cur_filter->fi_threshold) {
					pass = 0;
					cstate->stats_drop_fn(cstate->cookie);
					cstate->stats_process_fn(cstate->
								 cookie);
					for (i = 0; i < cur_fidx; i++) {
						j = pmElt(cur_perm, i);
						fdata->fd_filters[j].
						    fi_called++;
						fdata->fd_filters[j].
						    fi_cache_pass++;
						    
						/* 
				 		 * determine source of cache hit
				 	 	 */
						source_cache_hit(&fdata->fd_filters[j],
										 &id_sig, 
										 &change_attr,
										 cstate->qinfo,
										 NULL);
					}

					/* 
					 * credit passes in ceval_filters2, 
					 * so they are only counted once.
					 */
					cur_filter->fi_called++;
					cur_filter->fi_cache_drop++;
					cur_filter->fi_drop++;
					
					/* 
				 	 * determine when this hit was created 
				 	 * and update stats 
				 	 */
					source_cache_hit(cur_filter, &id_sig, 
									 &change_attr,
									 cstate->qinfo,
									 &entry_info);
				}

				/*
				 * modify change attr set 
				 */
				if (pass) {
					if (oattr_set != NULL)
						combine_attr_set(&change_attr,
								 oattr_set);
					if (oattr_fnum < MAX_FILTERS
					    && perm_num == 0) {
						pr_obj->filters[oattr_fnum] =
						    cur_filter->fi_name;
						memcpy(&pr_obj->
						       fsig[oattr_fnum],
						       &cur_filter->fi_sig,
						       sizeof(sig_val_t));
						memcpy(&pr_obj->
						       iattrsig[oattr_fnum],
						       &isig,
						       sizeof(sig_val_t));
						oattr_fnum++;
					}
				}
				rt_stop(&rt);
				time_ns = rt_nanos(&rt);

				fexec_update_prob(fdata, cur_fid,
					  		pmArr(fdata->fd_perm), cur_fidx,
					  		pass);

				char *sig_str = sig_string(&id_sig);
				log_message(LOGT_FILT, LOGL_TRACE,
					    "ceval_filters1(%s): CACHE HIT filter %s -> %d (%d), %lld ns",
					    sig_str, cur_filter->fi_name, conf,
					    cur_filter->fi_threshold, time_ns);
				free(sig_str);
			} else {
				hit = 0;
				rt_stop(&rt);
				time_ns = rt_nanos(&rt);
				char *sig_str = sig_string(&id_sig);
				log_message(LOGT_FILT, LOGL_TRACE,
					    "ceval_filters1(%s): CACHE MISS filter %s -> %d (%d), %lld ns",
					    sig_str, cur_filter->fi_name, conf,
					    cur_filter->fi_threshold, time_ns);
				free(sig_str);
			}

			cur_filter->fi_time_ns += time_ns;	
			stack_ns += time_ns;

			if (!hit) {
				break;
			}
		}
		if (hit) {
			free(change_attr.entry_data);
			break;
		}
		free(change_attr.entry_data);
	}
	if (hit) {
		// cached_perm[perm_num]->drop_rate++; /*XXX add later? */
	} else {
		/*
		 * XXX assume no overlapping among gid objects 
		 */
		if ((perm_done == 0) && (cached_perm_num < MAX_PERM_NUM)) {
			generate_new_perm(fdata->fd_po,
					  cached_perm[cached_perm_num - 1],
					  cur_fidx, &new_perm);
			for (i = 0; i < cached_perm_num; i++) {
				if (pmEqual(new_perm, cached_perm[i])) {
					perm_done = 1;
					pmDelete(new_perm);
					break;
				}
			}
			if (perm_done == 0) {
				pmPrint(new_perm, buf, BUFSIZ);
				cached_perm[cached_perm_num] = new_perm;
				cached_perm_num++;
			}
		}
	}

	log_message(LOGT_FILT, LOGL_TRACE,
		    "ceval_filters1:  done - total time is %lld", stack_ns);

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
	fdata->fd_avg_exec = (0.95 * fdata->fd_avg_exec) + (0.05 * temp);


	/*
	 * if pass, add the obj & oattr_fname list into the odisk queue 
	 */
	if (pass) {
		pr_obj->obj_name = objname;
		pr_obj->stack_ns = stack_ns;
		if (use_cache_oattr && dynamic_use_oattr()) {
			pr_obj->oattr_fnum = oattr_fnum;
		} else {
			pr_obj->oattr_fnum = 0;
		}
		odisk_pr_add(pr_obj);
	} else {
		free(pr_obj);
	}
	return pass;
}


void
ceval_wattr_stats(off_t len)
{
	if (fexec_active_filter != NULL) {
		fexec_active_filter->fi_added_bytes += len;
	}
}


int
ceval_filters2(obj_data_t *obj_handle, filter_data_t *fdata, int force_eval,
	       double *elapsed,	filter_exec_mode_t exec_mode,
	       query_info_t *qinfo,
	       void *cookie, int (*continue_cb) (void *cookie))
{
	filter_info_t  *cur_filter;
	int             conf;
	char            timebuf[BUFSIZ];
	int             err;
	size_t          asize;
	int             pass = 1;	/* return value */
	long int        rv;
	int             cur_fid,
	                cur_fidx;
	struct timeval  wstart;
	struct timeval  wstop;
	struct timezone tz;
	double          temp;
	rtimer_t        rt;
	u_int64_t       time_ns;	/* time for one filter */
	u_int64_t       stack_ns;	/* time for whole filter stack */
	cache_attr_set *oattr_set;
	char           *sig_str;

	sig_str = sig_string(&obj_handle->id_sig);
	log_message(LOGT_FILT, LOGL_DEBUG, "ceval_filters2(%s): Entering",
		    sig_str);
	free(sig_str);

	if (fdata->fd_num_filters == 0) {
		log_message(LOGT_FILT, LOGL_ERR, "ceval_filters2: no filters");
		return 1;
	}

	/*
	 * change the permutation if it's time for a change
	 */
	optimize_filter_order(fdata, &policy_arr[filter_exec_current_policy]);

	asize = sizeof(stack_ns);
	err = obj_read_attr(&obj_handle->attr_info, FLTRTIME,
			    &asize, (void *) &stack_ns);
	if (err != 0) {
		stack_ns = 0;
	}

	err = gettimeofday(&wstart, &tz);
	assert(err == 0);

	/* save initial object attributes */
	cache_set_init_attrs(&obj_handle->id_sig, &obj_handle->attr_info);

	for (cur_fidx = 0; cur_fidx < pmLength(fdata->fd_perm); cur_fidx++) {

		if ((pass == 0) && (fdata->full_eval == 0) && 
			 (!hybrid_mode_enabled || (exec_mode != FM_HYBRID)))
			break;
		/*
		 * in hybrid filter execution mode, continue
		 * executing tagged filters even if the object
		 * is to be discarded for this search
		 */
		if ((pass == 0) && 
			hybrid_mode_enabled && (exec_mode == FM_HYBRID) && 
			(fdata->hybrid_eval == 0))
			continue;

		cur_fid = pmElt(fdata->fd_perm, cur_fidx);
		cur_filter = &fdata->fd_filters[cur_fid];
		fexec_active_filter = cur_filter;

		oattr_set = NULL;

		/*
		 * See if the cache thread has already run this filter.
		 * The cache thread queues passed objects and tags
		 * them with a time attribute for each filter run.
		 */
		sprintf(timebuf, FLTRTIME_FN, cur_filter->fi_name);
		asize = sizeof(time_ns);
		err = obj_read_attr(&obj_handle->attr_info, timebuf,
				    &asize, (void *) &time_ns);
		if (err == 0) {
			cur_filter->fi_called++;
			cur_filter->fi_cache_pass++;
			cur_filter->fi_hits_intra_query++;

			char *sig_str1 = sig_string(&obj_handle->id_sig);
			char *sig_str2 = sig_string(&cur_filter->fi_sig);
			log_message(LOGT_FILT, LOGL_TRACE,
				    "ceval_filters2(%s): CACHE HIT filter %s (%s) PASS",
				    sig_str1,
				    cur_filter->fi_name,
				    sig_str2);
			free(sig_str1);
			free(sig_str2);
		} else {
			/*
			 * Look at the current filter bypass to see if we should 
			 * actually run it or pass it.  For the non-auto 
			 * partitioning, we still use the bypass values
			 * to determine how much of the allocation to run.
			 */
			if (force_eval == 0) {
				if ((fexec_autopart_type == AUTO_PART_BYPASS)
				    || (fexec_autopart_type ==
					AUTO_PART_NONE)) {
					rv = random();
					if (rv > cur_filter->fi_bpthresh) {
						pass = 1;
						break;
					}
				} else
				    if ((fexec_autopart_type ==
					 AUTO_PART_QUEUE)
					&& (cur_filter->fi_firstgroup)) {
					if ((*continue_cb) (cookie) == 0) {
						pass = 1;
						break;
					}
				}
			}

			cur_filter->fi_called++;

			/*
			 * initialize obj state for this call 
			 */
			obj_handle->cur_offset = 0;
			obj_handle->cur_blocksize = 1024;	/* XXX */

			/*
			 * run the filter and update pass 
			 */
			rt_init(&rt);
			rt_start(&rt);	/* assume only one thread here */

			assert(cur_filter->fi_eval_fp);

			/*
			 * mark beginning of filter eval into cache ring 
			 */
			if (add_cache_entries) {
				ocache_add_start(cur_filter->fi_name,
					 			&obj_handle->id_sig,
					 			cur_filter->cache_table,
					 			&cur_filter->fi_sig);
			}

			conf = cur_filter->fi_eval_fp(obj_handle,
						      cur_filter->
						      fi_filt_arg);

			/*
			 * mark end of filter eval into cache ring 
			 */
			if (add_cache_entries) {
				ocache_add_end(cur_filter->fi_name, 
								&obj_handle->id_sig, 
				 	      		conf, qinfo, exec_mode);
			}

			cur_filter->fi_compute++;

			/*
			 * get timing info and update stats 
			 */
			rt_stop(&rt);
			time_ns = rt_nanos(&rt);


			if (conf == -1) {
				cur_filter->fi_error++;
				pass = 0;
			} else if (conf < cur_filter->fi_threshold) {
				pass = 0;
			}

			char *sig_str1 = sig_string(&obj_handle->id_sig);
			char *sig_str2 = sig_string(&cur_filter->fi_sig);
			log_message(LOGT_FILT, LOGL_TRACE,
				    "ceval_filters2(%s): CACHE MISS filter %s (%s) %s, %lld ns",
				    sig_str1,
				    cur_filter->fi_name,
				    sig_str2,
				    pass?"PASS":"FAIL",
				    time_ns);
			free(sig_str1);
			free(sig_str2);
		}
		cur_filter->fi_time_ns += time_ns;
		stack_ns += time_ns;
		err = obj_write_attr(&obj_handle->attr_info, timebuf,
				     sizeof(time_ns), (void *) &time_ns);
		assert(err == 0);

		fexec_update_prob(fdata, cur_fid, pmArr(fdata->fd_perm),
				  cur_fidx, pass);

		if (!pass) {
			cur_filter->fi_drop++;
		} else {
			cur_filter->fi_pass++;
		}
	}


	if ((cur_fidx >= pmLength(fdata->fd_perm)) && pass) {
		obj_handle->remain_compute = 0.0;
		pass = 2;
	} else if ((cur_fidx < pmLength(fdata->fd_perm)) && pass) {
		float           avg;
		err = fexec_estimate_remaining(fdata, fdata->fd_perm,
					       cur_fidx, 0,
					       &obj_handle->remain_compute);
		err = fexec_estimate_remaining(fdata, fdata->fd_perm, 0, 0,
					       &avg);
		obj_handle->remain_compute /= avg;
	}

	oattr_sample();

	fexec_active_filter = NULL;

	sig_str = sig_string(&obj_handle->id_sig);
	log_message(LOGT_FILT, LOGL_TRACE,
		    "ceval_filters2(%s): %s total time %lld",
		    sig_str,
		    pass?"PASS":"FAIL",
		    stack_ns);
	free(sig_str);

	/*
	 * save the total time info attribute
	 */
	err = obj_write_attr(&obj_handle->attr_info,
			     FLTRTIME, sizeof(stack_ns), (void *) &stack_ns);
	if (err != 0) {
		printf("CHECK OBJECT %016llX ATTR FILE\n",
		       obj_handle->local_id);
	}

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

	return pass;
}


#define SAMPLE_NUM		100
// #define AJUST_RATE 5

static unsigned int    sample_counter = 0;
static int             oattr_percent = 80;	// start with using rate 50%
static double          opt_time = 1000000.0;	// initialize to an extrme large
					// value
static struct timeval  sample_start_time;
static struct timeval  sample_end_time;
static int             direction = -1;
static int             adjust = 5 * 4;

static void
sample_init()
{
	oattr_percent = 80;
	opt_time = 1000000.0;	// initialize to an extrme large value
	sample_counter = 0;
	direction = -1;
	adjust = 5 * 4;
}

static void
oattr_sample()
{
	int             err;
	struct timezone st_tz;
	double          temp;

	if (sample_counter == 0) {
		err = gettimeofday(&sample_start_time, &st_tz);
		assert(err == 0);
	}
	sample_counter++;
	if (sample_counter >= SAMPLE_NUM) {
		err = gettimeofday(&sample_end_time, &st_tz);
		assert(err == 0);
		temp = tv_diff(&sample_end_time, &sample_start_time);
		if (temp > opt_time) {
			oattr_percent -= (adjust * direction);	// first
			// getting
			// back
			direction = direction * (-1);
			if (adjust > 5) {
				adjust /= 2;
			}
		}
		if (oattr_percent >= 100) {
			oattr_percent = 100;
			direction = -1;
		}
		if (oattr_percent <= 0) {
			oattr_percent = 0;
			direction = 1;
		}
		opt_time = temp;
		oattr_percent += adjust * direction;
		sample_counter = 0;
	}
	return;
}

static int
dynamic_use_oattr()
{
	unsigned int    random;

	return (1);

	if (mdynamic_load == 0) {
		return (1);
	}

	random = 1 + (int) (100.0 * rand() / (RAND_MAX + 1.0));
	if (random <= oattr_percent) {
		// if(random <= 50) {
		return (1);
	} else {
		return (0);
	}
}
