/*
 *
 *
 *                          Diamond 1.0
 *
 *            Copyright (c) 2002-2004, Intel Corporation
 *                         All Rights Reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 *    * Neither the name of Intel nor the names of its contributors may
 *      be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
#include "lib_ocache.h"
// #include "cache_filter.h"

#define	MAX_FILTER_NUM	128
#define CACHE_DIR               "cache"
#define	MAX_PERM_NUM	5

/* XXX forward reference */
static void sample_init();
static int dynamic_use_oattr();
static void oattr_sample();

static permutation_t *cached_perm[MAX_PERM_NUM];
static int      cached_perm_num = 0;
static int      perm_done = 0;

static filter_info_t *active_filter = NULL;

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
	// { INDEP_POLICY, indep_new, indep_delete, indep_optimize, NULL },
	{INDEP_POLICY, indep_new, best_first_delete, best_first_optimize, NULL},
	{RANDOM_POLICY, random_new, NULL, NULL, NULL},
	{STATIC_POLICY, static_new, NULL, NULL, NULL},
	{NULL_POLICY, NULL, NULL, NULL, NULL}
};

unsigned int    use_cache_table = 1;
unsigned int    use_cache_oattr = 1;
unsigned int    cache_oattr_thresh = 10000;
unsigned int	 mdynamic_load = 1;

static void
mark_end()
{
	pr_obj_t       *pr_obj;

	pr_obj = (pr_obj_t *) malloc(sizeof(*pr_obj));
	assert( pr_obj != NULL );
	pr_obj->obj_id = 0;
	pr_obj->filters = NULL;
	pr_obj->fsig = NULL;
	pr_obj->iattrsig = NULL;
	pr_obj->oattr_fnum = -1;
	pr_obj->stack_ns = 0;
	odisk_pr_add(pr_obj);
}

static void    *
ceval_main(void *arg)
{
	ceval_state_t  *cstate = (ceval_state_t *) arg;
	uint64_t        oid;
	int             err;

	while (1) {
		pthread_mutex_lock(&ceval_mutex);
		while (search_active == 0) {
			ceval_blocked = 1;
			err = pthread_cond_wait(&active_cv, &ceval_mutex);
		}
		ceval_blocked = 0;
		pthread_mutex_unlock(&ceval_mutex);

		err = odisk_read_next_oid(&oid, cstate->odisk);

		if (err == 0) {
			ceval_filters1(oid, cstate->fdata, cstate, NULL);
		}

		if (err == ENOENT) {
			mark_end();
			search_active = 0;
		}
	}
}

/*
 * called when DEV_SEARCHLET 
 */
int
ceval_init_search(filter_data_t * fdata, ceval_state_t * cstate)
{
	filter_id_t     fid;
	filter_info_t  *cur_filt;
	int             err;
	uint64_t        tmp1, tmp2;
	char            buf[PATH_MAX];
	struct timeval  atime;
	struct timezone tz;

	err = gettimeofday(&atime, &tz);
	assert(err == 0);

	pthread_mutex_lock(&ceval_mutex);
	for (fid = 0; fid < fdata->fd_num_filters; fid++) {
		cur_filt = &fdata->fd_filters[fid];
		if (fid == fdata->fd_app_id) {
			continue;
		}
		cur_filt->fi_sig = (unsigned char *) malloc(16);
		assert(cur_filt->fi_sig != NULL);
		err =
			digest_cal(cur_filt->lib_name, cur_filt->fi_eval_name,
					   cur_filt->fi_numargs, cur_filt->fi_arglist,
					   cur_filt->fi_blob_len, cur_filt->fi_blob_data,
					   &cur_filt->fi_sig);
		assert(cur_filt->fi_sig != NULL);
		memcpy(&tmp1, cur_filt->fi_sig, sizeof(tmp1));
		memcpy(&tmp2, cur_filt->fi_sig + 8, sizeof(tmp2));
		// printf("filter %d, %s, signature %016llX%016llX\n", fid,
		// cur_filt->fi_eval_name, tmp1, tmp2);
		sprintf(buf, "%s/%s/%016llX%016llX", cstate->odisk->odisk_path,
				CACHE_DIR, tmp1, tmp2);
		err = mkdir(buf, 0x777);
		if (err && errno != EEXIST) {
			printf("fail to creat dir %s, err %d\n", buf, err);
		}
		ocache_read_file(cstate->odisk->odisk_path, cur_filt->fi_sig,
						 &cur_filt->cache_table, &atime);
	}

	cstate->fdata = fdata;
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

	new_state = (ceval_state_t *) malloc(sizeof(*new_state));
	assert(new_state != NULL);
	dctl_register_leaf(DEV_CACHE_PATH, "use_cache_table", DCTL_DT_UINT32,
					   dctl_read_uint32, dctl_write_uint32, &use_cache_table);
	dctl_register_leaf(DEV_CACHE_PATH, "use_cache_oattr", DCTL_DT_UINT32,
					   dctl_read_uint32, dctl_write_uint32, &use_cache_oattr);
   dctl_register_leaf(DEV_CACHE_PATH, "cache_oattr_thresh", DCTL_DT_UINT32,
                  dctl_read_uint32, dctl_write_uint32, &cache_oattr_thresh);
   dctl_register_leaf(DEV_CACHE_PATH, "mdynamic_load", DCTL_DT_UINT32,
                  dctl_read_uint32, dctl_write_uint32, &mdynamic_load);
                                                                                
	memset(new_state, 0, sizeof(*new_state));
	new_state->odisk = odisk;
	new_state->cookie = cookie;
	new_state->stats_drop_fn = stats_drop_fn;
	new_state->stats_process_fn = stats_process_fn;

	err = pthread_create(&new_state->ceval_thread_id, PATTR_DEFAULT,
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
	// printf("generate_new_perm %s\n", buf);
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
	struct 	timespec	ts;

	/* set search inactive and wait background
	 * thread to block.
	 */
	search_active = 0;
	while (ceval_blocked == 0) {
		ts.tv_sec = 0; 
		ts.tv_nsec = 10000000; 	/* 10 ms */
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
		ocache_stop_search(cur_filt->fi_sig);
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
	int             i, j;
	int            *list1, *list2;
	int             list1_num, list2_num;
	int             index;

	// printf("generate_new_perm fidx %d\n", fidx);
	if (copy == NULL)
		return (EINVAL);
	ptr = pmDup(copy);
	if (ptr == NULL)
		return (ENOMEM);

	list1 = (int *) malloc(sizeof(int) * pmLength(copy));
	assert( list1 != NULL);
	list2 = (int *) malloc(sizeof(int) * pmLength(copy));
	assert( list2 != NULL);
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

	// pmPrint(ptr, buf, BUFSIZ);
	// printf("generate_new_perm %s\n", buf);
	*new_perm = ptr;
	free(list1);
	free(list2);

	return (0);
}

int
ceval_filters1(uint64_t oid, filter_data_t * fdata, void *cookie,
			   int (*cb_func) (void *cookie, char *name,
							   int *pass, uint64_t * et))
{
	ceval_state_t  *cstate = (ceval_state_t *) cookie;
	filter_info_t  *cur_filter;
	int             conf;
	int             err;
	int             pass = 1;	/* return value */
	int             cur_fid, cur_fidx = 0;
	struct timeval  wstart;
	struct timeval  wstop;
	struct timezone tz;
	double          temp;
	rtimer_t        rt;
	u_int64_t       time_ns;	/* time for one filter */
	u_int64_t       stack_ns;	/* time for whole filter stack */
	cache_attr_set  change_attr;
	cache_attr_set *oattr_set;
	int             found;
	int             hit = 1;
	char          **filters, **fsig, **iattrsig;
	int             oattr_fnum = 0;
	char           *isig;
	pr_obj_t       *pr_obj;
	int             i, j;
	int             perm_num;
	permutation_t  *cur_perm, *new_perm;
	char            buf[BUFSIZ];

	// printf("ceval_filters1: obj %016llX\n",oid);

	fdata->obj_counter++;

	if (use_cache_table == 0) {
		pr_obj = (pr_obj_t *) malloc(sizeof(*pr_obj));
		assert( pr_obj != NULL);
		pr_obj->obj_id = oid;
		pr_obj->filters = NULL;
		pr_obj->fsig = NULL;
		pr_obj->iattrsig = NULL;
		pr_obj->oattr_fnum = 0;
		pr_obj->stack_ns = 0;
		odisk_pr_add(pr_obj);
		return (1);
	}

	if (fdata->fd_num_filters == 0) {
		log_message(LOGT_FILT, LOGL_ERR, "eval_filters: no filters");
		return 1;
	}

	filters = malloc(MAX_FILTER_NUM);
	assert( filters != NULL);
	fsig = malloc(MAX_FILTER_NUM);
	assert (fsig != NULL);
	iattrsig = malloc(MAX_FILTER_NUM);
	assert( iattrsig != NULL);

	stack_ns = 0;

	err = gettimeofday(&wstart, &tz);
	assert(err == 0);

	for (perm_num = 0; perm_num < cached_perm_num; perm_num++) {
		change_attr.entry_num = 0;
		change_attr.entry_data = malloc(ATTR_ENTRY_NUM * sizeof(char *));
		assert(change_attr.entry_data != NULL);

		cache_lookup0(oid, &change_attr, NULL);

		cur_perm = cached_perm[perm_num];
		assert(cur_perm != NULL);
		// pmPrint(cur_perm, buf, BUFSIZ);
		// printf("use perm %s\n", buf);
		hit = 1;
		/*
		 * here we do cache lookup based on object_id, filter signature and
		 * changed_attr_set. stop when we get a cache miss or object drop 
		 */
		for (cur_fidx = 0; pass && cur_fidx < pmLength(cur_perm); cur_fidx++) {
			cur_fid = pmElt(cur_perm, cur_fidx);
			cur_filter = &fdata->fd_filters[cur_fid];

			if (cb_func) {
				err =
					(*cb_func) (cookie, cur_filter->fi_name, &pass, &time_ns);
#define SANITY_NS_PER_FILTER (2 * 1000000000)

				assert(time_ns < SANITY_NS_PER_FILTER);
			} else {
				rt_init(&rt);
				rt_start(&rt);

				found = cache_lookup(oid, cur_filter->fi_sig,
									 cur_filter->cache_table, &change_attr,
									 &conf, &oattr_set, &isig);

				if (found) {
					/*
					 * get the cached output attr set and 
					 * modify changed attr set 
					 */
					if (conf < cur_filter->fi_threshold) {
						pass = 0;
						cstate->stats_drop_fn(cstate->cookie);
						cstate->stats_process_fn(cstate->cookie);
						for (i = 0; i < cur_fidx; i++) {
							j = pmElt(cur_perm, i);
							fdata->fd_filters[j].fi_called++;
							fdata->fd_filters[j].fi_cache_pass++;
						}
						cur_filter->fi_called++;
						cur_filter->fi_cache_drop++;
					}

					/*
					 * modify change attr set 
					 */
					if (pass) {
						if (oattr_set != NULL)
							combine_attr_set(&change_attr, oattr_set);
						if (isig != NULL && oattr_fnum < MAX_FILTER_NUM
							&& perm_num == 0) {
							filters[oattr_fnum] = cur_filter->fi_name;
							fsig[oattr_fnum] = cur_filter->fi_sig;
							iattrsig[oattr_fnum] = isig;
							oattr_fnum++;
						}
					}
					rt_stop(&rt);
					time_ns = rt_nanos(&rt);
					// printf("ceval_filters1: hit, time_ns %lld for filter
					// %s\n", time_ns, cur_filter->fi_name);

					log_message(LOGT_FILT, LOGL_TRACE,
								"eval_filters:  filter %s has val (%d) - threshold %d",
								cur_filter->fi_name, conf,
								cur_filter->fi_threshold);

				} else {
					hit = 0;
					rt_stop(&rt);
					time_ns = rt_nanos(&rt);
					// printf("ceval_filters1: miss, time_ns %lld for filter
					// %s\n", time_ns, cur_filter->fi_name);
				}

			}
			cur_filter->fi_time_ns += time_ns;	/* update filter stats */
			stack_ns += time_ns;
			if (!pass) {
				cur_filter->fi_drop++;
				// printf("ceval_filters1: drop obj %016llX\n", oid);
			} else {
				cur_filter->fi_pass++;
			}

			fexec_update_prob(fdata, cur_fid, pmArr(fdata->fd_perm),
							  cur_fidx, pass);

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
			generate_new_perm(fdata->fd_po, cached_perm[cached_perm_num - 1],
							  cur_fidx, &new_perm);
			for (i = 0; i < cached_perm_num; i++) {
				if (pmEqual(new_perm, cached_perm[i])) {
					perm_done = 1;
					printf("no new perm\n");
					pmDelete(new_perm);
					break;
				}
			}
			if (perm_done == 0) {
				pmPrint(new_perm, buf, BUFSIZ);
				printf("generate perm %s\n", buf);
				cached_perm[cached_perm_num] = new_perm;
				cached_perm_num++;
			}
		}
	}

	/*
	 * XXX 
	 */
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

	/*
	 * if pass, add the obj & oattr_fname list into the odisk queue 
	 */
	if (pass) {
		if ( use_cache_oattr && dynamic_use_oattr() ) {
			pr_obj = (pr_obj_t *) malloc(sizeof(*pr_obj));
			assert( pr_obj != NULL);
			pr_obj->obj_id = oid;
			pr_obj->filters = filters;
			pr_obj->fsig = fsig;
			pr_obj->iattrsig = iattrsig;
			pr_obj->oattr_fnum = oattr_fnum;
			pr_obj->stack_ns = stack_ns;
			odisk_pr_add(pr_obj);
		} else {
			pr_obj = (pr_obj_t *) malloc(sizeof(*pr_obj));
			assert( pr_obj != NULL);
			pr_obj->obj_id = oid;
			pr_obj->filters = filters;
			pr_obj->fsig = fsig;
			pr_obj->iattrsig = iattrsig;
			pr_obj->oattr_fnum = 0;
			pr_obj->stack_ns = stack_ns;
			odisk_pr_add(pr_obj);
		}
	} else {
		free(filters);
		free(fsig);
		free(iattrsig);
	}
	return pass;
}


void
ceval_wattr_stats(off_t len)
{
	if (active_filter != NULL) {
		active_filter->fi_added_bytes += len;
	}
}


int
ceval_filters2(obj_data_t * obj_handle, filter_data_t * fdata, int force_eval,
			   void *cookie, int (*continue_cb) (void *cookie),
			   int (*cb_func) (void *cookie, char *name,
							   int *pass, uint64_t * et))
{
	filter_info_t  *cur_filter;
	int             conf;
	lf_obj_handle_t out_list[16];
	char            timebuf[BUFSIZ];
	int             err;
	off_t           asize;
	int             pass = 1;	/* return value */
	int             rv;
	int             cur_fid, cur_fidx;
	struct timeval  wstart;
	struct timeval  wstop;
	struct timezone tz;
	double          temp;
	rtimer_t        rt;
	u_int64_t       time_ns;	/* time for one filter */
	u_int64_t       stack_ns;	/* time for whole filter stack */
	cache_attr_set  change_attr;
	cache_attr_set *oattr_set;
	int             lookup;
	int             miss = 0;
	int				 oattr_flag=0;

	log_message(LOGT_FILT, LOGL_TRACE, "eval_filters: Entering");
	// printf("ceval_filters2: obj %016llX\n",obj_handle->local_id);

	if (fdata->fd_num_filters == 0) {
		log_message(LOGT_FILT, LOGL_ERR, "eval_filters: no filters");
		return 1;
	}

	/*
	 * change the permutation if it's time for a change
	 */
	optimize_filter_order(fdata, &policy_arr[filter_exec.current_policy]);

	asize = sizeof(stack_ns);
	err = obj_read_attr(&obj_handle->attr_info, FLTRTIME,
						&asize, (void *) &stack_ns);
	if (err != 0) {
		stack_ns = 0;
	}

	err = gettimeofday(&wstart, &tz);
	assert(err == 0);

	change_attr.entry_num = 0;
	change_attr.entry_data = malloc(ATTR_ENTRY_NUM * sizeof(char *));
	assert(change_attr.entry_data != NULL);

	cache_lookup0(obj_handle->local_id, &change_attr, &obj_handle->attr_info);

	for (cur_fidx = 0; pass && cur_fidx < pmLength(fdata->fd_perm);
		 cur_fidx++) {
		cur_fid = pmElt(fdata->fd_perm, cur_fidx);
		cur_filter = &fdata->fd_filters[cur_fid];
		active_filter = cur_filter;

		oattr_set = NULL;

		sprintf(timebuf, FLTRTIME_FN, cur_filter->fi_name);
		asize = sizeof(time_ns);
		err = obj_read_attr(&obj_handle->attr_info, timebuf,
							&asize, (void *) &time_ns);

		/*
		 * we still want to use cached results for filters after cache 
		 * missing point at which we stopped in ceval_filters1, if we can 
		 */
		if ((miss == 0) && (use_cache_table)) {
			lookup = cache_lookup2(obj_handle->local_id,
								   cur_filter->fi_sig,
								   cur_filter->cache_table, &change_attr,
								   &conf, &oattr_set, &oattr_flag, err);
		} else {
			lookup = ENOENT;
			oattr_flag = 0;
		}

		if ((lookup == 0) && (conf < cur_filter->fi_threshold)) {
			pass = 0;
			cur_filter->fi_drop++;
			cur_filter->fi_called++;
			break;
		}

		/*
		 * Look at the current filter bypass to see if we should 
		 * actually run it or pass it.  For the non-auto 
		 * partitioning, we still use the bypass values
		 * to determine how much of * the allocation to run.
		 */
		if (err == 0) {
			cur_filter->fi_called++;
			cur_filter->fi_cache_pass++;
		} else {
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
			 * XXX build the out list appropriately
			 */
			out_list[0] = obj_handle;

			/*
			 * initialize obj state for this call
			 */
			obj_handle->cur_offset = 0;
			obj_handle->cur_blocksize = 1024;	/* XXX */

			/*
			 * run the filter and update pass
			 */
			if (cb_func) {
				err =
					(*cb_func) (cookie, cur_filter->fi_name, &pass, &time_ns);
#define SANITY_NS_PER_FILTER (2 * 1000000000)

				assert(time_ns < SANITY_NS_PER_FILTER);
			} else {
				rt_init(&rt);
				rt_start(&rt);	/* assume only one thread here */

				assert(cur_filter->fi_eval_fp);
				/*
				 * write the evaluation start message into the cache ring 
				 */
            //if( (oattr_flag != 0) && ((cur_filter->fi_time_ns/1000) <= (cur_filter->fi_compute * cache_oattr_thresh)) ) {
				if( (oattr_flag != 0) && (cur_filter->fi_time_ns <= (cur_filter->fi_added_bytes*cache_oattr_thresh)) ) {
					oattr_flag = 0;
            }
/*
				 else {
					int com_rate;
					if(cur_filter->fi_added_bytes != 0 )
						com_rate = cur_filter->fi_time_ns/cur_filter->fi_added_bytes;
					else
						com_rate = 0;
					printf("filter %s com_rate %d\n", cur_filter->fi_name, com_rate);
				}
*/
				ocache_add_start(cur_filter->fi_name, obj_handle->local_id,
						cur_filter->cache_table, lookup, oattr_flag, cur_filter->fi_sig);

				conf = cur_filter->fi_eval_fp(obj_handle, 1, out_list,
											  cur_filter->fi_filt_arg);
				/*
				 * write the evaluation end message into the cache ring 
				 */
				ocache_add_end(cur_filter->fi_name, obj_handle->local_id,
							   conf);

				cur_filter->fi_compute++;
				/*
				 * get timing info and update stats
				 */
				rt_stop(&rt);
				time_ns = rt_nanos(&rt);

				if (conf < cur_filter->fi_threshold) {
					pass = 0;
				}
				// printf("eval_filters: filter %s has val (%d) - threshold
				// %d\n", cur_filter->fi_name, conf,
				// cur_filter->fi_threshold);

				log_message(LOGT_FILT, LOGL_TRACE,
							"eval_filters:  filter %s has val (%d) - threshold %d",
							cur_filter->fi_name, conf,
							cur_filter->fi_threshold);
			}
			cur_filter->fi_time_ns += time_ns;	/* update filter stats */
			stack_ns += time_ns;
			err = obj_write_attr(&obj_handle->attr_info, timebuf,
								 sizeof(time_ns), (void *) &time_ns);
			if (err != 0) {
				printf("CHECK OBJECT %016llX ATTR FILE\n",
					   obj_handle->local_id);
			}
			assert(err == 0);
		}

#ifdef PRINT_TIME
		printf("\t\tmeasured: %f secs\n", rt_time2secs(time_ns));
		printf("\t\tfilter %s: %f secs cumulative, %f s avg\n",
			   cur_filter->fi_name, rt_time2secs(cur_filter->fi_time_ns),
			   rt_time2secs(cur_filter->fi_time_ns) / cur_filter->fi_called);
#endif

		// XXX printf("ceval2: update prob, pass %d \n", pass);
		fexec_update_prob(fdata, cur_fid, pmArr(fdata->fd_perm),
						  cur_fidx, pass);

		if (!pass) {
			cur_filter->fi_drop++;
			break;
		} else {
			cur_filter->fi_pass++;
		}

		if ((lookup == ENOENT) && (miss == 0)) {
			lookup = cache_wait_lookup(obj_handle,
									   cur_filter->fi_sig,
									   cur_filter->cache_table, &change_attr,
									   &oattr_set);
		}
			if (lookup == 0) {
				if (oattr_set != NULL)
					combine_attr_set(&change_attr, oattr_set);
			} else {
				miss = 1;
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
		err = fexec_estimate_remaining(fdata, fdata->fd_perm, 0, 0, &avg);
		obj_handle->remain_compute /= avg;
	}

	oattr_sample();

	active_filter = NULL;
	log_message(LOGT_FILT, LOGL_TRACE,
				"eval_filters:  done - total time is %lld", stack_ns);

	/*
	 * save the total time info attribute
	 */
	err = obj_write_attr(&obj_handle->attr_info,
						 FLTRTIME, sizeof(stack_ns), (void *) &stack_ns);
	if (err != 0) {
		printf("CHECK OBJECT %016llX ATTR FILE\n", obj_handle->local_id);
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

	free(change_attr.entry_data);

	return pass;
}

#define SAMPLE_NUM		16
//#define AJUST_RATE		5

unsigned int sample_counter = 0;
int oattr_percent = 80; //start with using rate 50%
double opt_time=1000000.0; //initialize to an extrme large value
struct timeval  sample_start_time, sample_end_time;
int direction=-1;
int	adjust=5*4;

static 
void sample_init()
{
	oattr_percent = 80;
	opt_time = 1000000.0; //initialize to an extrme large value
	sample_counter = 0;
	direction=-1;
	adjust = 5*4;
}

static void oattr_sample()
{
	int err;
	struct timezone st_tz;
	double temp;

	if( sample_counter == 0 ) {
		err = gettimeofday(&sample_start_time, &st_tz);
		assert(err == 0);
	}
	sample_counter++;
	if( sample_counter >= SAMPLE_NUM ) {
		err = gettimeofday(&sample_end_time, &st_tz);
		assert(err == 0);
		temp = tv_diff(&sample_end_time, &sample_start_time);
		//printf("oattr_percent is %u, adjust is %u, temp %f, opt_time %f\n", oattr_percent, adjust, temp, opt_time);
		if( temp > opt_time ) {
			oattr_percent -= (adjust*direction); //first getting back
			direction = direction * (-1);
			if( adjust > 5 ) {
				adjust /= 2;
			}
		}
		if( oattr_percent >= 100 ) {
			oattr_percent = 100;
			direction = -1;
		}
		if( oattr_percent <= 0 ) {
			oattr_percent = 0;
			direction = 1;
		}
		opt_time = temp;
		oattr_percent += adjust*direction;
		sample_counter=0;
	}
	return;
}

static 
int dynamic_use_oattr()
{
	unsigned int random;

	if( mdynamic_load == 0 ) {
		return(1);
	}

	random = 1 + (int) (100.0 * rand()/(RAND_MAX+1.0) );
	if(random <= oattr_percent) {
	//if(random <= 50) {
		return(1);
	} else {
		return(0);
	}
}
