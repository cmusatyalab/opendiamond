
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

static int      host_cycles = 0;


float
get_active_searches()
{
    /*
     * XXXX fix 
     */
    return (1.0);
}



float
get_disk_cycles()
{
    uint64_t        val;
    float           fval;
    int             err;

    err = r_cpu_freq(&val);
    assert(err == 0);

    fval = (float) val;
    return (fval);
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
fexec_set_bypass_none(filter_data_t * fdata)
{
    int             i;
    for (i = 0; i < fdata->fd_num_filters; i++) {
        fdata->fd_filters[i].fi_bpthresh = RAND_MAX;
    }
}


void
fexec_set_bypass_greedy(filter_data_t * fdata, permutation_t * perm,
                        float target)
{
    int             i;
    filter_prob_t  *fprob;
    double          pass = 1;   /* cumul pass rate */
    double          old_cost = 0;   /* cumulitive cost so far */
    double          new_cost = 0;   /* new cost so far */
    double          ratio;
    filter_info_t  *info;
    int             n;

    for (i = 0; i < pmLength(perm); i++) {
        double          c;      /* cost of this filter */
        double          p;      /* pass rate for this filter in this pos */

        info = &fdata->fd_filters[pmElt(perm, i)];

        /*
         * if we are already above the threshold, then all of these should be 
         * run at the host. 
         */
        if (old_cost > target) {
            info->fi_bpthresh = -1;
        } else {
            /*
             * pass = pass rate of all filters before this one 
             */
            c = info->fi_time_ns;
            n = info->fi_called;

            new_cost = old_cost + pass * c / n;

            /*
             * if new > target, then we have crossed the theshold,
             * compute the ratio of these to process.
             */

            if (new_cost > target) {

                ratio = (target - old_cost) / (new_cost - old_cost);
                assert(ratio >= 0.0 && ratio <= 1.0);

                info->fi_bpthresh = (int) ((float) RAND_MAX * ratio);
            } else {
                /*
                 * if we are below threshold, run everything. 
                 */
                info->fi_bpthresh = RAND_MAX;
            }

            /*
             * lookup this permutation 
             */
            /*
             * XXX 
             */
            fprob = fexec_lookup_prob(fdata, pmElt(perm, i), i, pmArr(perm));
            if (fprob) {
                p = (double) fprob->num_pass / fprob->num_exec;
            } else {
				/* unknown value, assume 1.0 */
				p = 1.0;
            }

            assert(p >= 0 && p <= 1.0);
            /*
             * for now we don't need to consider the bypass
             * ratios because  
             */
            pass *= p;

#define SMALL_FRACTION (0.00001)
            /*
             * don't let it go to zero XXX 
             */
            if (pass < SMALL_FRACTION) {
                pass = SMALL_FRACTION;
            }
            old_cost = new_cost;
        }
    }
    return;
}



/*
 * This computes the byapss by just splitting the first percentage.
 */
void
fexec_set_bypass_trivial(filter_data_t * fdata, permutation_t * perm,
                          double ratio)
{
    int             i;
    filter_info_t  *info;

    /*
     * for the first filtre run int locally
     */
    info = &fdata->fd_filters[pmElt(perm, 0)];
    if (ratio >= 1.0) {
        info->fi_bpthresh = RAND_MAX;
    } else {
        info->fi_bpthresh = (int) ((float) RAND_MAX * ratio);
    }


    for (i = 1; i < pmLength(perm); i++) {
        info = &fdata->fd_filters[pmElt(perm, i)];
        info->fi_bpthresh = RAND_MAX;
    }
    return;
}

/*
 * Computes the network-optimal ("hybrid") bypass distribution.
 * (nizhner 05/11/2004)
 */

/*
 * XXX
 * Assume an array of these is allocated for each permutation
 * when it is created, of size pmLength(perm)+1
 *
 */
typedef struct {
  /*
   * The cumulative CPU cost of the sub-permutation up to (but not including)
   * this filter. (e.g., 0 for the first element, the full cost of the
   * searchlet for the last.)
   */
  double dcost;
  /*
   * The network cost (bytes transmitted) under the greedy distribution
   * when the target CPU cost is precisely dcost.
   */
  double greedy_ncost;
  int i;  /* filter unit begin */
  int j;  /* filter unit end */
  double c_i;  /* cumulative CPU cost up to filter i */
  double c_j;  /* cumulative CPU cost up to filter j */
} bp_hybrid_state_t;

void
fexec_set_bypass_hybrid(filter_data_t *fdata, permutation_t *perm, float target_ms)
{
  int i, j;
  filter_info_t * info;
  filter_prob_t * fprob;
  bp_hybrid_state_t* hstate;
  double dcost = 0;
  double this_cost;
  double p, pass = 1;
  double maxbytes = 100.0;
  double ratio;
	
  hstate = (bp_hybrid_state_t *) malloc(sizeof(*hstate) * (pmLength(perm)+1));
  assert(hstate != NULL);

  /*
   * Reconstruct the cost function of the greedy distribution.
   */
  for(i=0; i < pmLength(perm); i++) {
    int n;

    hstate[i].dcost = dcost;
    hstate[i].greedy_ncost = pass * maxbytes;

    info = &fdata->fd_filters[pmElt(perm, i)];
    n = info->fi_called;
	if (n == 0) {
		n = 1;
	}
    maxbytes += (double) info->fi_added_bytes / (double) n;
	this_cost = pass * ((double)info->fi_time_ns / (double) n); /* update cumulative cost */
	/* we don't want the incremental cost to be zero */
	if (this_cost == 0.0) {
		this_cost = SMALL_FRACTION;
	}
    dcost += this_cost;

    /* obtain conditional pass rate */
    fprob = fexec_lookup_prob(fdata, pmElt(perm, i), i, pmArr(perm)); 
    if(fprob) {
      p = (double)fprob->num_pass / fprob->num_exec;
    } else {
      /* really no data, return an error */
      /* XXX */
	  p = 1.0;
    }
    
    assert(p >= 0 && p <= 1.0);
    
    pass *= p;
    if (pass < SMALL_FRACTION) {
      pass = SMALL_FRACTION;
    }
  }
  hstate[i].dcost = dcost;
  hstate[i].greedy_ncost = pass * maxbytes;

  
  /*
   * Identify optimal breakdown into unit subsequences.
   */
  for (i=0; i < pmLength(perm); i++) {
    double delta, lowest_delta;
    int best_j=0, k;

    lowest_delta = 999999999999.0;  /* XXX */
    for(j=i+1; j <= pmLength(perm); j++) {
      /* compute reduction factor for candidate filter unit */
      assert((hstate[j].dcost - hstate[i].dcost)>0.0);

      delta = (hstate[j].greedy_ncost - hstate[i].greedy_ncost) /
	(hstate[j].dcost - hstate[i].dcost);
      if(delta < lowest_delta){
	/* record candidate unit parameters */
	lowest_delta = delta;
	best_j = j;
      }
    }

    /*
     * Found optimal unit starting with filter i.  
     */
    for(k=i; k<best_j; k++){
      hstate[k].i = i;
      hstate[k].j = best_j;
      hstate[k].c_i = hstate[i].dcost;
      hstate[k].c_j = hstate[best_j].dcost;
    }

    i = best_j-1; /* next unit */
  }

  
  /*
   * Compute the greedy distribution on unit subsequences.
   */
  for(i=0; i <= pmLength(perm); i++) {
    if(hstate[i].dcost>target_ms){
      i--;
      break;
    }
  }

  if(i>=pmLength(perm)){
    i = pmLength(perm) - 1;
  }

  for(j=0; j<hstate[i].i; j++){
    (fdata->fd_filters[pmElt(perm, j)]).fi_bpthresh = RAND_MAX;
  }

  ratio =  (target_ms - hstate[i].c_i) / (hstate[i].c_j - hstate[i].c_i);

  /* cap ratio at 1 */
  if(ratio>1.0)
    ratio = 1.0;

  (fdata->fd_filters[pmElt(perm, hstate[i].i)]).fi_bpthresh =
    (int)((double)RAND_MAX * ratio);

  for(j=hstate[i].i+1; j<hstate[i].j; j++){
    (fdata->fd_filters[pmElt(perm, j)]).fi_bpthresh = RAND_MAX;
  }
  for(j=hstate[i].j; j<pmLength(perm); j++){
    (fdata->fd_filters[pmElt(perm, j)]).fi_bpthresh = -1;
  }

  free(hstate);
  return;
}




int
fexec_update_bypass(filter_data_t * fdata, double ratio)
{
    double          avg_cost;
    double          target_cost;
    int             err;

    err = fexec_estimate_cost(fdata, fdata->fd_perm, 1, 0, &avg_cost);

    /*
     * If we have an error, we can't compute the cost, so
     * we don't send anything to the host yet.
     */
    /*
     * XXX using diferent splitting algorithm ?? 
     */
    if (err == 1) {
		printf("failed to compute cost \n");
        fexec_set_bypass_none(fdata);
        return (0);
    }

    target_cost = avg_cost * ratio;

    switch (fexec_bypass_type) {
	case  BP_NONE:
       	fexec_set_bypass_none(fdata);
		break;

	case BP_SIMPLE:
    		fexec_set_bypass_trivial(fdata, fdata->fd_perm, ratio);
		break;
		
	case BP_GREEDY:
    		fexec_set_bypass_greedy(fdata, fdata->fd_perm, target_cost);
		break;

	case BP_HYBRID:
    		fexec_set_bypass_hybrid(fdata, fdata->fd_perm, target_cost);
		break;
	

    }

    return (0);
}
