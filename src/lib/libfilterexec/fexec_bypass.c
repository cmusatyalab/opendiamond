/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 4
 *
 *  Copyright (c) 2002-2005 Intel Corporation
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
#include <limits.h>

#include "diamond_consts.h"
#include "diamond_types.h"
#include "lib_tools.h"
#include "lib_odisk.h"
#include "lib_log.h"
#include "sys_attr.h"
#include "lib_filterexec.h"
#include "filter_priv.h"
#include "fexec_stats.h"


/*
 * This forces all filters to be run at the disk.
 */

static void
fexec_set_bypass_none(filter_data_t * fdata)
{
	int             i;
	for (i = 0; i < fdata->fd_num_filters; i++) {
		fdata->fd_filters[i].fi_bpthresh = RAND_MAX;
	}
}

static void
fexec_set_grouping_none(filter_data_t * fdata)
{
	int             i;

	for (i = 0; i < fdata->fd_num_filters; i++) {
		fdata->fd_filters[i].fi_firstgroup = 0;
	}
}



#define SMALL_FRACTION (0.00001)

static void
fexec_set_bypass_greedy(filter_data_t * fdata, permutation_t * perm,
			float target)
{
	int             i;
	filter_prob_t  *fprob;
	float           pass = 1.0;	/* cumul pass rate */
	float           old_cost = 0;	/* cumulitive cost so far */
	float           new_cost = 0;	/* new cost so far */
	float           ratio;
	filter_info_t  *info;
	int             n;

	for (i = 0; i < pmLength(perm); i++) {
		float           c;	/* cost of this filter */
		float           p;	/* pass rate for this filter in this
					 * pos */

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

			if (n < FSTATS_VALID_NUM) {
				c = FSTATS_UNKNOWN_COST;
				n = FSTATS_UNKNOWN_NUM;
			}
			new_cost = old_cost + pass * c / n;

			/*
			 * if new > target, then we have crossed the theshold,
			 * compute the ratio of these to process.
			 */

			if (new_cost > target) {

				ratio =
				    (target - old_cost) / (new_cost -
							   old_cost);
				assert(ratio >= 0.0 && ratio <= 1.0);

				info->fi_bpthresh =
				    (int) ((float) RAND_MAX * ratio);
			} else {
				/*
				 * if we are below threshold, run everything. 
				 */
				info->fi_bpthresh = RAND_MAX;
			}

			/*
			 * lookup this permutation 
			 */
#ifdef	XXX
			/*
			 * XXX 
			 */
			fprob =
			    fexec_lookup_prob(fdata, pmElt(perm, i), i,
					      pmArr(perm));
			if (fprob) {
				if (fprob->num_exec < FSTATS_VALID_NUM) {
					p = FSTATS_UNKNOWN_PROB;
				} else {
					p = (float) fprob->num_pass /
					    fprob->num_exec;
				}
			} else {
				/*
				 * unknown value, use default 
				 */
				p = FSTATS_UNKNOWN_PROB;
			}
#else
			if (info->fi_called < FSTATS_VALID_NUM) {
				p = FSTATS_UNKNOWN_PROB;
			} else {
				p = (double) info->fi_pass /
				    (double) info->fi_called;
			}
#endif
			assert(p >= 0 && p <= 1.0);
			/*
			 * for now we don't need to consider the bypass
			 * ratios because  
			 */
			pass *= p;

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


static void
fexec_set_grouping_greedy(filter_data_t * fdata, permutation_t * perm)
{
	int             i;

	for (i = 0; i < fdata->fd_num_filters; i++) {
		fdata->fd_filters[i].fi_firstgroup = 1;
	}
}



/*
 * This computes the byapss by just splitting the first percentage.
 */
static void
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

static void
fexec_set_grouping_trivial(filter_data_t * fdata, permutation_t * perm)
{
	int             i;

	/*
	 * only the first filter is first in the group.
	 */
	fdata->fd_filters[pmElt(perm, 0)].fi_firstgroup = 1;

	for (i = 1; i < pmLength(perm); i++) {
		fdata->fd_filters[pmElt(perm, i)].fi_firstgroup = 0;
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
	double          dcost;
	/*
	 * The network cost (bytes transmitted) under the greedy distribution
	 * when the target CPU cost is precisely dcost.
	 */
	double          greedy_ncost;
	int             i;	/* filter unit begin */
	int             j;	/* filter unit end */
	double          c_i;	/* cumulative CPU cost up to filter i */
	double          c_j;	/* cumulative CPU cost up to filter j */
} bp_hybrid_state_t;

static void
fexec_set_bypass_hybrid(filter_data_t * fdata, permutation_t * perm,
			float target_ms)
{
	int             i,
	                j;
	filter_info_t  *info;
	filter_prob_t  *fprob;
	bp_hybrid_state_t *hstate;
	double          dcost = 0;
	double          this_cost;
	double          p,
	                pass = 1.0;
	double          maxbytes = 300000.0;
	double          ratio;

	hstate = (bp_hybrid_state_t *)
	    malloc(sizeof(*hstate) * (pmLength(perm) + 1));
	assert(hstate != NULL);

	/*
	 * Reconstruct the cost function of the greedy distribution.
	 */
	for (i = 0; i < pmLength(perm); i++) {
		int             n;
		double          c;

		hstate[i].dcost = dcost;
		hstate[i].greedy_ncost = pass * maxbytes;

		// XXX printf("hstate[%d] dc = %f greed %f\n",
		// XXX i, hstate[i].dcost, hstate[i].greedy_ncost);
		// XXX printf("%f \n", pass);

		info = &fdata->fd_filters[pmElt(perm, i)];
		n = info->fi_called;
		c = info->fi_time_ns;
		if (n < FSTATS_VALID_NUM) {
			c = FSTATS_UNKNOWN_COST;
			n = FSTATS_UNKNOWN_NUM;
		}

		/*
		 * update cumulative cost 
		 */
		maxbytes += (double) info->fi_added_bytes / (double) n;
		this_cost = pass * ((double) c / (double) n);

		/*
		 * we don't want the incremental cost to be zero 
		 */
		if (this_cost == 0.0) {
			this_cost = SMALL_FRACTION;
		}
		dcost += this_cost;

#ifdef	XXX
		/*
		 * obtain conditional pass rate 
		 */
		fprob =
		    fexec_lookup_prob(fdata, pmElt(perm, i), i, pmArr(perm));
		if (fprob) {
			printf("num exec %d pass %d\n", fprob->num_exec,
			       fprob->num_pass);
			if (fprob->num_exec < FSTATS_VALID_NUM) {
				p = FSTATS_UNKNOWN_PROB;
			} else {
				p = (double) fprob->num_pass /
				    (double) fprob->num_exec;
			}
		} else {
			/*
			 * unknown value, use default 
			 */
			printf("!!! no lookup \n");
			p = FSTATS_UNKNOWN_PROB;
		}
		printf("p %f \n", p);
#else

		if (info->fi_called < FSTATS_VALID_NUM) {
			p = FSTATS_UNKNOWN_PROB;
		} else {
			p = (double) info->fi_pass / (double) info->fi_called;
		}

#endif

		assert(p >= 0 && p <= 1.0);

		pass *= p;
		if (pass < SMALL_FRACTION) {
			pass = SMALL_FRACTION;
		}
	}
	hstate[i].dcost = dcost;
	hstate[i].greedy_ncost = pass * maxbytes;
	// XXX printf("hstate[%d] dc = %f greed %f\n",
	// XXX i, hstate[i].dcost, hstate[i].greedy_ncost);

	/*
	 * Identify optimal breakdown into unit subsequences.
	 */
	for (i = 0; i < pmLength(perm); i++) {
		double          delta,
		                lowest_delta;
		int             best_j = 0,
		    k;

		lowest_delta = 999999999999.0;	/* XXX */
		for (j = i + 1; j <= pmLength(perm); j++) {
			/*
			 * compute reduction factor for candidate filter unit 
			 */
			assert((hstate[j].dcost - hstate[i].dcost) > 0.0);

			delta =
			    (hstate[j].greedy_ncost -
			     hstate[i].greedy_ncost) / (hstate[j].dcost -
							hstate[i].dcost);
			if (delta < lowest_delta) {
				/*
				 * record candidate unit parameters 
				 */
				lowest_delta = delta;
				best_j = j;
			}
			// XXX printf("i=%d j=%d delta=%f low=%f\n", i,j,
			// delta,
			// XXX lowest_delta);
		}

		/*
		 * Found optimal unit starting with filter i.  
		 */
		for (k = i; k < best_j; k++) {
			hstate[k].i = i;
			hstate[k].j = best_j;
			hstate[k].c_i = hstate[i].dcost;
			hstate[k].c_j = hstate[best_j].dcost;
		}

		// XXX printf("i=%d bestj %d\n", i, best_j);

		i = best_j - 1;	/* next unit */
	}

	// XXX printf("\n\nsetting hybrid dist\n");

	/*
	 * Compute the greedy distribution on unit subsequences.
	 */
	for (i = 0; i <= pmLength(perm); i++) {
		if (hstate[i].dcost > target_ms) {
			i--;
			break;
		}
	}

	if (i >= pmLength(perm)) {
		i = pmLength(perm) - 1;
	}

	for (j = 0; j < hstate[i].i; j++) {
		// XXX printf(" %d is RANDMAX (%s)\n", j,
		// XXX (fdata->fd_filters[pmElt(perm, j)]).fi_name);
		(fdata->fd_filters[pmElt(perm, j)]).fi_bpthresh = RAND_MAX;
	}

	ratio = (target_ms - hstate[i].c_i) / (hstate[i].c_j - hstate[i].c_i);

	/*
	 * cap ratio at 1 
	 */
	if (ratio > 1.0) {
		ratio = 1.0;
	}
	// XXX printf("target ration %f \n", ratio);

	(fdata->fd_filters[pmElt(perm, hstate[i].i)]).fi_bpthresh =
	    (int) ((double) RAND_MAX * ratio);

	// XXX printf(" %d is %d (%s)\n", hstate[i].i,
	// XXX (int)((double)RAND_MAX * ratio),
	// XXX (fdata->fd_filters[pmElt(perm, hstate[i].i)]).fi_name);

	for (j = hstate[i].i + 1; j < hstate[i].j; j++) {
		(fdata->fd_filters[pmElt(perm, j)]).fi_bpthresh = RAND_MAX;
		// XXX printf(" %d is %d (%s)\n", j, RAND_MAX,
		// XXX (fdata->fd_filters[pmElt(perm, j)]).fi_name);
	}
	for (j = hstate[i].j; j < pmLength(perm); j++) {
		(fdata->fd_filters[pmElt(perm, j)]).fi_bpthresh = -1;
		// XXX printf(" %d is %d (%s)\n", j, -1,
		// XXX (fdata->fd_filters[pmElt(perm, j)]).fi_name);
	}

	free(hstate);
	return;
}

static void
fexec_set_grouping_hybrid(filter_data_t * fdata, permutation_t * perm,
			  float target_ms)
{
	int             i,
	                j;
	filter_info_t  *info;
	filter_prob_t  *fprob;
	bp_hybrid_state_t *hstate;
	double          dcost = 0;
	double          this_cost;
	double          p,
	                pass = 1;
	double          maxbytes = 100.0;

	hstate =
	    (bp_hybrid_state_t *) malloc(sizeof(*hstate) *
					 (pmLength(perm) + 1));
	assert(hstate != NULL);

	/*
	 * Reconstruct the cost function of the greedy distribution.
	 */
	for (i = 0; i < pmLength(perm); i++) {
		int             n;

		hstate[i].dcost = dcost;
		hstate[i].greedy_ncost = pass * maxbytes;

		info = &fdata->fd_filters[pmElt(perm, i)];
		n = info->fi_called;
		if (n == 0) {
			n = 1;
		}
		maxbytes += (double) info->fi_added_bytes / (double) n;

		/*
		 * update cumulative cost 
		 */
		this_cost = pass * ((double) info->fi_time_ns / (double) n);
		/*
		 * we don't want the incremental cost to be zero 
		 */
		if (this_cost == 0.0) {
			this_cost = SMALL_FRACTION;
		}
		dcost += this_cost;

		/*
		 * obtain conditional pass rate 
		 */
		fprob =
		    fexec_lookup_prob(fdata, pmElt(perm, i), i, pmArr(perm));
		if (fprob) {
			p = (double) fprob->num_pass / fprob->num_exec;
		} else {
			/*
			 * really no data, return an error 
			 */
			/*
			 * XXX 
			 */
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
	for (i = 0; i < pmLength(perm); i++) {
		double          delta,
		                lowest_delta;
		int             best_j = 0,
		    k;

		lowest_delta = 999999999999.0;	/* XXX */
		for (j = i + 1; j <= pmLength(perm); j++) {
			/*
			 * compute reduction factor for candidate filter unit 
			 */
			assert((hstate[j].dcost - hstate[i].dcost) > 0.0);

			delta =
			    (hstate[j].greedy_ncost -
			     hstate[i].greedy_ncost) / (hstate[j].dcost -
							hstate[i].dcost);
			if (delta < lowest_delta) {
				/*
				 * record candidate unit parameters 
				 */
				lowest_delta = delta;
				best_j = j;
			}
		}

		/*
		 * Found optimal unit starting with filter i.  
		 */

		(fdata->fd_filters[pmElt(perm, i)]).fi_firstgroup = 1;

		for (k = i + 1; k < best_j; k++)
			(fdata->fd_filters[pmElt(perm, k)]).fi_firstgroup = 0;

		i = best_j - 1;	/* next unit */
	}

	free(hstate);
	return;
}




int
fexec_update_bypass(filter_data_t * fdata, double ratio)
{
	float           avg_cost;
	float           target_cost;
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
	case BP_NONE:
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

int
fexec_update_grouping(filter_data_t * fdata, double ratio)
{
	float           avg_cost;
	float           target_cost;
	int             err;


	switch (fexec_bypass_type) {
	case BP_NONE:
		fexec_set_grouping_none(fdata);
		break;

	case BP_SIMPLE:
		fexec_set_grouping_trivial(fdata, fdata->fd_perm);
		break;

	case BP_GREEDY:
		fexec_set_grouping_greedy(fdata, fdata->fd_perm);
		break;

	case BP_HYBRID:
		err =
		    fexec_estimate_cost(fdata, fdata->fd_perm, 1, 0,
					&avg_cost);
		assert(err == 0);
		target_cost = avg_cost * ratio;
		fexec_set_grouping_hybrid(fdata, fdata->fd_perm, target_cost);
		break;
	}

#ifdef XXX
	{
		int             i;
		printf("bypass: ");
		for (i = 0; i < pmLength(fdata->fd_perm); i++)
			printf("%lf ",
			       (double) ((fdata->
					  fd_filters[pmElt
						     (fdata->fd_perm,
						      i)]).fi_bpthresh) /
			       (double) RAND_MAX);

		printf("\n");
	}
#endif

	return (0);
}
