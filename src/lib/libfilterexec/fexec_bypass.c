
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>
#include <assert.h>
#include <stdint.h>
#ifdef linux
#include <values.h>
#endif

#include "lib_od.h"
#include "lib_odisk.h"
#include "lib_log.h"
#include "lib_searchlet.h"
#include "attr.h"
#include "queue.h"
#include "rstat.h"
#include "filter_exec.h"
#include "filter_priv.h"
#include "rtimer.h"
#include "rgraph.h"
#include "rcomb.h"
#include "fexec_stats.h"

static int  host_cycles = 0;

/* XXX */
extern int fexec_fixed_split;
extern int fexec_fixed_ratio;


float
get_active_searches()
{
    /* XXXX fix */
    return(1.0);
}



float
get_disk_cycles()
{
    uint64_t    val;
    float       fval;
    int         err;

    err = r_cpu_freq(&val);
    assert(err == 0);

    fval = (float)val;
    return(fval);
}


/*
 * This is called when we get a new request from the host
 * to change the number of cycles sent from the host.
 */
void
fexec_update_hostcyles(int new)
{
    host_cycles = new;
}


/*
 * This forces all filters to be run at the disk.
 */

void
fexec_set_bypass_none(filter_data_t *fdata)
{
    int     i;
    for (i=0; i < fdata->fd_num_filters; i++) {
        fdata->fd_filters[i].fi_bpthresh = RAND_MAX;
    }
}


void
fexec_set_bypass_target(filter_data_t *fdata, permutation_t *perm, float target)
{
    int             i;
    filter_prob_t * fprob;
    double          pass = 1;		/* cumul pass rate */
    double          old_cost = 0;   /* cumulitive cost so far */
    double          new_cost = 0;	/* new cost so far */
    double          ratio;
    filter_info_t * info;
    int             n;

    for(i=0; i < pmLength(perm); i++) {
        double c;			/* cost of this filter */
        double p;			/* pass rate for this filter in this pos */

        info = &fdata->fd_filters[pmElt(perm, i)];

        /* if we are already above the threshold,
         * then all of these should be run at the host.
         */
        if (old_cost > target) {
            info->fi_bpthresh = -1;
        } else {
            /* pass = pass rate of all filters before this one */
            c = info->fi_time_ns;
            n = info->fi_called;

            new_cost = old_cost + pass * c / n;	

            /*
             * if new > target, then we have crossed the theshold,
             * compute the ratio of these to process.
             */

            if (new_cost > target) {

                ratio = (target - old_cost)/(new_cost - old_cost);
                assert(ratio >= 0.0 && ratio <= 1.0);

                info->fi_bpthresh = (int)((float)RAND_MAX * ratio);

                printf("split:  ratio %f  bp_thresh %d max %d \n",
					ratio, info->fi_bpthresh, RAND_MAX);
            } else {
                /* if we are below threshold, run everything. */
                info->fi_bpthresh = RAND_MAX;
            }
    
            /* lookup this permutation */
            /* XXX */
            fprob = fexec_lookup_prob(fdata, pmElt(perm, i), i, pmArr(perm)); 
            if(fprob) {
                p = (double)fprob->num_pass / fprob->num_exec;
            } else {
                /* really no data, return an error */
                /* XXX */
                assert(0);
                return;
            }
    
            assert(p >= 0 && p <= 1.0);
            /*
             * for now we don't need to consider the bypass
             * ratios because  
             */
            pass *= p;

#define SMALL_FRACTION (0.00001)
            /* don't let it go to zero XXX */
            if (pass < SMALL_FRACTION) {
                pass = SMALL_FRACTION;
            }
            old_cost = new_cost;
        }
    }
    return;
}



int
fexec_update_bypass(filter_data_t *fdata)
{
    double       avg_cost;
    float       target_cost;
    float       num_searches;
    int         disk_cycles;
    int         err;

    err = fexec_compute_cost(fdata, fdata->fd_perm, 1, &avg_cost);

    /*
     * If we have an error, we can't compute the cost, so
     * we don't send anything to the host yet.
     */
    if (err == 1) {
        printf("XXXXXXXXXXX bypass is none \n");
        fexec_set_bypass_none(fdata);
        return(0);
    }

    num_searches = get_active_searches();

    disk_cycles = get_disk_cycles();

    disk_cycles /= num_searches;

    /*
	 * Compute the target goal for here.
	 */
	if (fexec_fixed_split) {
		target_cost = avg_cost * ((float)fexec_fixed_ratio/100.0);
		printf("new target cost %f -> %f \n", avg_cost, target_cost);
  	} else {
    		target_cost = 
		((float) (disk_cycles)/(disk_cycles + host_cycles)) * avg_cost;
    	/* XXX debug */
    	target_cost = ((float)  avg_cost * 0.70);
	}

    printf("setting target %f of %f \n", target_cost, avg_cost);
    fexec_set_bypass_target(fdata, fdata->fd_perm, target_cost);

    return(0);
}


