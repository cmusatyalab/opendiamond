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
#include "cache_filter.h"
#include "rtimer.h"
#include "rgraph.h"
#include "fexec_stats.h"
#include "fexec_opt.h"
#include "lib_ocache.h"

#define	MAX_FILTER_NUM	128

static int search_active = 0;
static pthread_mutex_t  ceval_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t   active_cv = PTHREAD_COND_INITIALIZER; /*active*/

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

static void *
mark_end()
{
    pr_obj_t * pr_obj;

	pr_obj = (pr_obj_t *) malloc( sizeof(*pr_obj) );
	pr_obj->obj_id = 0;
	pr_obj->filters = NULL;
	pr_obj->oattr_fname = NULL;
	pr_obj->oattr_fnum = -1;
	pr_obj->stack_ns = 0;
	odisk_pr_add(pr_obj);
}

static void *
ceval_main(void *arg)
{
   ceval_state_t  * cstate = (ceval_state_t *)arg;
   uint64_t oid;
   int err;

   printf("ceval_main start\n");
   while (1) {
	pthread_mutex_lock(&ceval_mutex);
	while (search_active == 0) {
		err = pthread_cond_wait(&active_cv, &ceval_mutex);
	}
	pthread_mutex_unlock(&ceval_mutex);

	err = odisk_read_next_oid(&oid, cstate->odisk);

	if( err == 0 )
		ceval_filters1(oid, cstate->fdata, cstate, NULL);
	if( err == ENOENT ) {
   		printf("ceval_main search done\n");
		mark_end();
		search_active = 0;
	}
   }
}

int
ceval_init_search(filter_data_t * fdata, odisk_state_t *odisk)
{
    filter_id_t     fid;
    filter_info_t  *cur_filt;
    int err;
    ceval_state_t    *new_state;
    uint64_t tmp;

    for (fid = 0; fid < fdata->fd_num_filters; fid++) {
        cur_filt = &fdata->fd_filters[fid];
        if (fid == fdata->fd_app_id) {
            continue;
        }
        cur_filt->fi_sig = (unsigned char *)malloc(16);
        err = digest_cal(cur_filt->lib_name, cur_filt->fi_eval_name, cur_filt->fi_numargs, cur_filt->fi_arglist, cur_filt->fi_blob_len, cur_filt->fi_blob_data, &cur_filt->fi_sig);
	memcpy( &tmp, cur_filt->fi_sig, sizeof(tmp) );
	printf("filter %s, signature %016llX\n", cur_filt->fi_eval_name, tmp);
    }

    new_state = (ceval_state_t *)malloc(sizeof(*new_state));
    if (new_state == NULL) {
	return(ENOMEM);
    }
    memset(new_state, 0, sizeof(*new_state));
    new_state->fdata = fdata;
    new_state->odisk = odisk;

    err = pthread_create(&new_state->ceval_thread_id, PATTR_DEFAULT, 
	ceval_main, (void *) new_state);

    return (0);
}

int
ceval_start()
{
    pthread_mutex_lock(&ceval_mutex);
    search_active = 1;
    pthread_cond_signal(&active_cv);
    pthread_mutex_unlock(&ceval_mutex);

    return (0);
}

int
ceval_filters1(uint64_t oid, filter_data_t * fdata, void *cookie,
			 int (*cb_func) (void *cookie, char *name,
                                           int *pass, uint64_t * et))
{
    filter_info_t  *cur_filter;
    int             conf;
    int             err;
    int             pass = 1;   /* return value */
    int             cur_fid,
                    cur_fidx;
    struct timeval  wstart;
    struct timeval  wstop;
    struct timezone tz;
    double          temp;
    rtimer_t        rt;
    u_int64_t       time_ns;    /* time for one filter */
    u_int64_t       stack_ns;   /* time for whole filter stack */
    cache_attr_set  change_attr;
    cache_attr_set  *oattr_set;
    int found;
    int hit=1;
    char **filters;
    char **oattr_fname;
    int oattr_fnum=0;
    char *fpath;
    pr_obj_t * pr_obj;

    //printf("ceval_filters1: obj %016llX\n",oid);
    log_message(LOGT_FILT, LOGL_TRACE, "eval_filters: Entering");

    fdata->obj_counter++;

    if (fdata->fd_num_filters == 0) {
        log_message(LOGT_FILT, LOGL_ERR, "eval_filters: no filters");
        return 1;
    }

    /*
     * change the permutation if it's time for a change 
     * XXX: may use another function to consider cache info later
     */
    optimize_filter_order(fdata, &policy_arr[filter_exec.current_policy]);

    stack_ns = 0;

    err = gettimeofday(&wstart, &tz);
    assert(err == 0);

    /* if the attr has not been changed, the change_attr is empty initially;
     * otherwise, the change_attr includes all attributes read in from
     * the attr file. 
     * XXX add check if attr changed later */
    change_attr.entry_num = 0;
    change_attr.entry_data = malloc(ATTR_ENTRY_NUM * sizeof(char *) );
    change_attr.entry_data[0] = NULL;

    filters = malloc( MAX_FILTER_NUM );
    oattr_fname = malloc( MAX_FILTER_NUM );
    /* here we do cache lookup based on object_id, filter signature and 
     * changed_attr_set.
     * stop when we get a cache miss or object drop */
    for (cur_fidx = 0; pass && hit && cur_fidx < pmLength(fdata->fd_perm);
         cur_fidx++) {
        cur_fid = pmElt(fdata->fd_perm, cur_fidx);
        cur_filter = &fdata->fd_filters[cur_fid];

        if (cb_func) {
            err = (*cb_func) (cookie, cur_filter->fi_name, &pass, &time_ns);
#define SANITY_NS_PER_FILTER (2 * 1000000000)
            assert(time_ns < SANITY_NS_PER_FILTER);
        } else {
            rt_init(&rt);
            rt_start(&rt);     

	    found = ocache_lookup(oid, cur_filter->fi_sig, &change_attr, &conf, &oattr_set, &fpath);

	    if( found ) {
		/* get the cached output attr set and modify changed attr set */
            	if (conf < cur_filter->fi_threshold) {
                	pass = 0;
			//printf("drop obj %016llX with filter %s, conf %d, thresh %d\n", oid, cur_filter->fi_name, conf, cur_filter->fi_threshold);
            	}

		/*modify change attr set */
		if( pass ) {
			if( oattr_set != NULL )
				combine_attr_set(&change_attr, oattr_set);
	    		if( fpath != NULL && oattr_fnum < MAX_FILTER_NUM ) {
				filters[oattr_fnum] = cur_filter->fi_name;
				oattr_fname[oattr_fnum] = fpath;
				oattr_fnum++;
			}
		}
            	rt_stop(&rt);
            	time_ns = rt_nanos(&rt);
		//printf("ceval_filters1: hit, time_ns %lld for filter %s\n", time_ns, cur_filter->fi_name);

            	log_message(LOGT_FILT, LOGL_TRACE,
                        "eval_filters:  filter %s has val (%d) - threshold %d",
                        cur_filter->fi_name, conf, cur_filter->fi_threshold);

	    } else {
		hit = 0;
            	rt_stop(&rt);
            	time_ns = rt_nanos(&rt);
		//printf("ceval_filters1: miss, time_ns %lld for filter %s\n", time_ns, cur_filter->fi_name);
	    }

        }
        cur_filter->fi_time_ns += time_ns;  /* update filter stats */
	stack_ns += time_ns;
        if (!pass) {
            cur_filter->fi_drop++;
        } else {
            cur_filter->fi_pass++;
        }

        fexec_update_prob(fdata, cur_fid, pmArr(fdata->fd_perm), cur_fidx,
                          pass);

    }

    /* XXX */
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

    /* if pass, add the obj & oattr_fname list into the odisk queue */
    if( pass ) {
	pr_obj = (pr_obj_t *) malloc( sizeof(*pr_obj) );
	pr_obj->obj_id = oid;
	pr_obj->filters = filters;
	pr_obj->oattr_fname = oattr_fname;
	pr_obj->oattr_fnum = oattr_fnum;
	pr_obj->stack_ns = stack_ns;
    	//printf("ceval_filters1: pr_add obj %016llX\n",oid);
	odisk_pr_add(pr_obj);
    } else {
	free(filters);
	free(oattr_fname);
    }
    /* free change_attr_set */
    free(change_attr.entry_data);

    return pass;
}

int
ceval_filters2(obj_data_t * obj_handle, filter_data_t * fdata, int force_eval,
             void *cookie, int (*continue_cb)(void *cookie),
                         int (*cb_func) (void *cookie, char *name,
                                           int *pass, uint64_t * et))
{
    filter_info_t  *cur_filter;
    int             conf;
    lf_obj_handle_t out_list[16];
    char            timebuf[BUFSIZ];
    int             err;
    off_t           asize;
    int             pass = 1;   /* return value */
    int             rv;
    int             cur_fid,
                    cur_fidx;
    struct timeval  wstart;
    struct timeval  wstop;
    struct timezone tz;
    double          temp;
    rtimer_t        rt;
    u_int64_t       time_ns;    /* time for one filter */
    u_int64_t       stack_ns;   /* time for whole filter stack */
    cache_attr_set  change_attr;
    cache_attr_set  *oattr_set;
    int lookup;
    int miss=0;
    char *fpath;

    //printf("ceval_filters2: obj %016llX\n",obj_handle->local_id);
    log_message(LOGT_FILT, LOGL_TRACE, "eval_filters: Entering");

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
    change_attr.entry_data = malloc(ATTR_ENTRY_NUM * sizeof(char *) );
    change_attr.entry_data[0] = NULL;

    for (cur_fidx = 0; pass && cur_fidx < pmLength(fdata->fd_perm);
         cur_fidx++) {
        cur_fid = pmElt(fdata->fd_perm, cur_fidx);
        cur_filter = &fdata->fd_filters[cur_fid];
	oattr_set = NULL;

	sprintf(timebuf, FLTRTIME_FN, cur_filter->fi_name);
	asize = sizeof(time_ns);
	err = obj_read_attr(&obj_handle->attr_info, timebuf,
			&asize, (void *) &time_ns);

	/* we still want to use cached results for filters after cache missing
	 * point at which we stopped in ceval_filters1, if we can */
	if( miss == 0 ) {
		lookup = ocache_lookup2(obj_handle->local_id, cur_filter->fi_sig, &change_attr, &conf, &oattr_set, &fpath, err);
	} else {
		lookup = ENOENT;
	}

	if( (lookup == 0) && (conf < cur_filter->fi_threshold) ) {
		pass = 0;
            	cur_filter->fi_drop++;
	   	cur_filter->fi_called++;
		break;
	}

        /*
         * Look at the current filter bypass to see if we should actually
         * run it or pass it.  For the non-auto partitioning, we
                 * we still use the bypass values to determine how much of
                 * the allocation to run.
         */
	if( err == 0 ) {
	   cur_filter->fi_called++;
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
                                if ((*continue_cb)(cookie) == 0) {
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
           obj_handle->cur_blocksize = 1024;   /* XXX */

           /*
            * run the filter and update pass
            */
           if (cb_func) {
           	err = (*cb_func) (cookie, cur_filter->fi_name, &pass, &time_ns);
#define SANITY_NS_PER_FILTER (2 * 1000000000)
            	assert(time_ns < SANITY_NS_PER_FILTER);
           } else {
            	rt_init(&rt);
            	rt_start(&rt);      /* assume only one thread here */

            	assert(cur_filter->fi_eval_fp);
            	/* write the evaluation start message into the cache ring */
            	ocache_add_start(cur_filter->fi_name, obj_handle->local_id, cur_filter->fi_sig, lookup, fpath);

            	conf = cur_filter->fi_eval_fp(obj_handle, 1, out_list,
                                cur_filter->fi_filt_arg);
           	/* write the evaluation end message into the cache ring */
            	ocache_add_end(cur_filter->fi_name, obj_handle->local_id, conf);

            	/*
             	 * get timing info and update stats
            	 */
            	rt_stop(&rt);
            	time_ns = rt_nanos(&rt);
		
		if (conf < cur_filter->fi_threshold) {
			pass = 0;
		}

           	log_message(LOGT_FILT, LOGL_TRACE,
                        "eval_filters:  filter %s has val (%d) - threshold %d",
                        cur_filter->fi_name, conf, cur_filter->fi_threshold);
	   }
           cur_filter->fi_time_ns += time_ns;  /* update filter stats */
           stack_ns += time_ns;
           obj_write_attr(&obj_handle->attr_info, timebuf,
                       sizeof(time_ns), (void *) &time_ns);
        }

#ifdef PRINT_TIME
        printf("\t\tmeasured: %f secs\n", rt_time2secs(time_ns));
        printf("\t\tfilter %s: %f secs cumulative, %f s avg\n",
               cur_filter->fi_name, rt_time2secs(cur_filter->fi_time_ns),
               rt_time2secs(cur_filter->fi_time_ns) / cur_filter->fi_called);
#endif

        if (!pass) {
            cur_filter->fi_drop++;
	    break;
        } else {
            cur_filter->fi_pass++;
        }

	if( (lookup == ENOENT) && (miss == 0) ) {
		lookup = ocache_wait_lookup(obj_handle, cur_filter->fi_sig, &change_attr, &oattr_set);
		if( lookup == 0 ) {
			if( oattr_set != NULL )
				combine_attr_set(&change_attr, oattr_set);
		} else {
			miss = 1;
		}
	}
        fexec_update_prob(fdata, cur_fid, pmArr(fdata->fd_perm), cur_fidx,
                          pass);

    }

        if ((cur_fidx >= pmLength(fdata->fd_perm)) && pass) {
                pass = 2;
        }

    log_message(LOGT_FILT, LOGL_TRACE,
                "eval_filters:  done - total time is %lld", stack_ns);

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

    /* free change_attr_set */
    free(change_attr.entry_data);

    return pass;
}
