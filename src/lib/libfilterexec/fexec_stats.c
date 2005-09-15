/*
 * 	Diamond (Release 1.0)
 *      A system for interactive brute-force search
 *
 *      Copyright (c) 2002-2005, Intel Corporation
 *      All Rights Reserved
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

static char const cvsid[] = "$Header$";

// #define VERBOSE 1
#ifdef VERBOSE
#define VERB if(1)
#else
#define VERB if(0)
#endif

/*
 * Comparison function for qsort that compares to filter_id_t.
 */
int
id_comp(const void *data1, const void *data2)
{
	filter_id_t     d1,
	d2;

	d1 = *(filter_id_t *) data1;
	d2 = *(filter_id_t *) data2;

	if (d1 < d2) {
		return (-1);
	} else if (d1 > d2) {
		return (1);
	} else {
		return (0);
	}
}

/*
 * This function walks through the list of filters and resets
 * all the statistics assocaited with each one.  This is typically
 * called when a new search is started.
 */

void
fexec_clear_stats(filter_data_t * fdata)
{
	int             i;

	for (i = 0; i < fdata->fd_num_filters; i++) {
		fdata->fd_filters[i].fi_called = 0;
		fdata->fd_filters[i].fi_drop = 0;
		fdata->fd_filters[i].fi_pass = 0;
		fdata->fd_filters[i].fi_error = 0;
		fdata->fd_filters[i].fi_time_ns = 0;
		fdata->fd_filters[i].fi_cache_drop = 0;
		fdata->fd_filters[i].fi_cache_pass = 0;
		fdata->fd_filters[i].fi_compute = 0;
		fdata->fd_filters[i].fi_added_bytes = 0;
	}

}

/*
 * Get the statistics for each of the filters.
 */

int
fexec_get_stats(filter_data_t * fdata, int max, filter_stats_t * fstats)
{
	filter_info_t  *cur_filt;
	filter_stats_t *cur_stat;
	int             i;

	if (fdata == NULL) {
		return (-1);
	}
	/*
	 * XXX keep the handle somewhere 
	 */
	for (i = 0; i < fdata->fd_num_filters; i++) {
		cur_filt = &fdata->fd_filters[i];

		/*
		 * if we are out of space return an error 
		 */
		if (i > max) {
			return (-1);
		}

		cur_stat = &fstats[i];

		strncpy(cur_stat->fs_name, cur_filt->fi_name, MAX_FILTER_NAME);
		cur_stat->fs_name[MAX_FILTER_NAME - 1] = '\0';
		cur_stat->fs_objs_processed = cur_filt->fi_called;
		cur_stat->fs_objs_dropped = cur_filt->fi_drop;

		/* JIAYING */
		cur_stat->fs_objs_cache_dropped = cur_filt->fi_cache_drop;
		cur_stat->fs_objs_cache_passed = cur_filt->fi_cache_pass;
		cur_stat->fs_objs_compute = cur_filt->fi_compute;
		/* JIAYING */
		if (cur_filt->fi_called != 0) {
			cur_stat->fs_avg_exec_time =
			    cur_filt->fi_time_ns / cur_filt->fi_called;
			//printf("filter %s was called %d times, time_ns %lld\n", cur_filt->fi_name, cur_filt->fi_called, cur_filt->fi_time_ns);
		} else {
			cur_stat->fs_avg_exec_time = 0;
			//printf("filter %s has 0 fi_called\n", cur_filt->fi_name);
		}

	}
	return (0);
}


unsigned int
fexec_hash_prob(filter_id_t cur_filt, int num_prev,
                const filter_id_t * sorted_list)
{
	unsigned long v = 0;
	int		i;
	/*
	 * XXX LH XXX 
	 */
	v = (unsigned long) cur_filt;
	for (i=0; i < num_prev; i++) {
		v = ((unsigned long)sorted_list[i]) + (v << 6) + (v << 16) - v;
	}

	v = v % PROB_HASH_BUCKETS;
	return (v);
}


/*
 * This looks up a the probabilty given a filter and a sorted
 * list of other filters that have been run before this.
 */
filter_prob_t  *
fexec_find_prob(filter_data_t * fdata, filter_id_t cur_filt, int num_prev,
                const filter_id_t * sorted_list)
{
	int             hash;
	filter_prob_t  *cur_node;
	int             datalen;

	datalen = sizeof(filter_id_t) * num_prev;

	hash = fexec_hash_prob(cur_filt, num_prev, sorted_list);

	LIST_FOREACH(cur_node, &fdata->fd_prob_hash[hash], prob_link) {
		if ((cur_node->num_prev == num_prev) &&
		    (cur_node->cur_fid == cur_filt) &&
		    (memcmp(sorted_list, cur_node->prev_id, datalen) == 0)) {
			return (cur_node);
		}
	}

	return (NULL);
}

/*
 * This looks up a probabilty given a filter and an unsorted list
 * of previous elements.
 */
filter_prob_t  *
fexec_lookup_prob(filter_data_t * fdata, filter_id_t cur_filt, int num_prev,
                  const filter_id_t * unsorted_list)
{
	filter_prob_t  *match;
	filter_id_t    *sorted_list;

	/*
	 * we need it one larger for union stats 
	 */
	sorted_list =
	    (filter_id_t *) malloc(sizeof(*sorted_list) * (num_prev + 1));
	assert(sorted_list != NULL);

	memcpy(sorted_list, unsorted_list, (num_prev * sizeof(filter_id_t)));

	qsort(sorted_list, num_prev, sizeof(filter_id_t), id_comp);

	match = fexec_find_prob(fdata, cur_filt, num_prev, sorted_list);
	free(sorted_list);

	return (match);
}


static filter_prob_t *
fexec_new_prob(filter_data_t * fdata, filter_id_t cur_filt, int num_prev,
               filter_id_t * sorted_list)
{
	filter_prob_t  *new_node;
	int             datalen;
	int             hash;

	hash = fexec_hash_prob(cur_filt, num_prev, sorted_list);

	datalen = sizeof(filter_id_t) * num_prev;

	new_node = (filter_prob_t *) malloc(FILTER_PROB_SIZE(num_prev));
	assert(new_node != NULL);


	new_node->cur_fid = cur_filt;
	memcpy(new_node->prev_id, sorted_list, datalen);

	new_node->num_pass = 0;
	new_node->num_exec = 0;
	new_node->num_prev = num_prev;

	LIST_INSERT_HEAD(&fdata->fd_prob_hash[hash], new_node, prob_link);
	return (new_node);
}




void
fexec_update_prob(filter_data_t * fdata, filter_id_t cur_filt,
                  const filter_id_t * prev_list, int num_prev, int pass)
{
	filter_id_t    *sorted_list;
	filter_prob_t  *prob;

	/*
	 * we need it one larger for union stats 
	 */
	sorted_list =
	    (filter_id_t *) malloc(sizeof(*sorted_list) * (num_prev + 1));
	assert(sorted_list != NULL);

	memcpy(sorted_list, prev_list, (num_prev * sizeof(filter_id_t)));
	qsort(sorted_list, num_prev, sizeof(filter_id_t), id_comp);

	/*
	 * lookup the prob data structure or allocatea new one 
	 */
	prob = fexec_find_prob(fdata, cur_filt, num_prev, sorted_list);
	if (prob == NULL) {
		prob = fexec_new_prob(fdata, cur_filt, num_prev, sorted_list);
		assert(prob != NULL);
	}
	prob->num_exec++;
	if (pass) {
		prob->num_pass++;
	}

	/*
	 * keep the total stats for this union of items 
	 */
	sorted_list[num_prev] = cur_filt;
	qsort(sorted_list, (num_prev + 1), sizeof(filter_id_t), id_comp);

	prob = fexec_find_prob(fdata, INVALID_FILTER_ID, (num_prev + 1),
	                       sorted_list);
	if (prob == NULL) {
		prob = fexec_new_prob(fdata, INVALID_FILTER_ID, (num_prev + 1),
		                      sorted_list);
		assert(prob != NULL);
	}
	prob->num_exec++;
	if (pass) {
		prob->num_pass++;
	}
	free(sorted_list);
	return;
}


/*
 * debug function that will probably go away 
 */
void
fexec_dump_prob(filter_data_t * fdata)
{
	int             hash,
	j;
	filter_prob_t  *cur_node;

	for (hash = 0; hash < PROB_HASH_BUCKETS; hash++) {
		LIST_FOREACH(cur_node, &fdata->fd_prob_hash[hash], prob_link) {
			fprintf(stdout, "%d -> ", cur_node->cur_fid);
			for (j = 0; j < cur_node->num_prev; j++) {
				fprintf(stdout, "%d:", cur_node->prev_id[j]);
			}

			fprintf(stdout, " = pass %d total %d \n", cur_node->num_pass,
			        cur_node->num_exec);
		}
	}

}

#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

#define SIGNIFICANT_NUMBER(g)  MAX(2,(g)*4)

/*
 * evalute the cost of given permutation using the data that is available.
 */

int
fexec_compute_cost(filter_data_t * fdata, permutation_t * perm, int gen,
                   int indep, float *cost)
{
	int             i;
	filter_prob_t  *fprob;
	float           pass = 1;   /* cumul pass rate */
	float           totalcost = 0;  /* = utility */
	filter_info_t  *info;
	int             n;

	/*
	 * NB: this assumes that filter_id_t and pelt_t are the same type XXX 
	 */
	assert(sizeof(pelt_t) == sizeof(filter_id_t));

	for (i = 0; i < pmLength(perm); i++) {
		float           c;      /* cost of this filter */
		float           p;      /* pass rate for this filter in this pos */
		/*
		 * pass = pass rate of all filters before this one 
		 */
		info = &fdata->fd_filters[pmElt(perm, i)];
		c = info->fi_time_ns;
		n = info->fi_called;
		if (n < SIGNIFICANT_NUMBER(gen)) {
			return 1;
		}

		totalcost += pass * c / n;  /* prev cumul pass * curr cost */

		/*
		 * lookup this permutation 
		 */
		/*
		 * XXX 
		 */
		if (indep) {
			/*
			 * pretend there's no context 
			 */
			fprob = fexec_lookup_prob(fdata, pmElt(perm, i), 0, NULL);
		} else {
			fprob = fexec_lookup_prob(fdata, pmElt(perm, i), i, pmArr(perm));
		}
		if (fprob) {
			/*
			 * not enough data 
			 */
			if (fprob->num_exec < SIGNIFICANT_NUMBER(gen)) {
				return 1;
			}
			p = (float) fprob->num_pass / fprob->num_exec;
		} else {
			/*
			 * really no data, return an error 
			 */
			return 1;
		}

		assert(p >= 0 && p <= 1.0);
		pass *= p;
#define SMALL_FRACTION (0.00001)
		/*
		 * don't let it go to zero XXX 
		 */
		if (pass < SMALL_FRACTION) {
			pass = SMALL_FRACTION;
		}
	}

	*cost = totalcost;
	return 0;
}

#define SMALL_FRACTION (0.00001)


int
fexec_estimate_cost(filter_data_t * fdata, permutation_t * perm, int gen,
                    int indep, float *cost)
{
	int             i;
	filter_prob_t  *fprob;
	float           pass = 1;   /* cumul pass rate */
	float           totalcost = 0;  /* = utility */
	filter_info_t  *info;
	int             n;

	/*
	 * NB: this assumes that filter_id_t and pelt_t are the same type XXX 
	 */
	assert(sizeof(pelt_t) == sizeof(filter_id_t));

	for (i = 0; i < pmLength(perm); i++) {
		float           c;      /* cost of this filter */
		float           p;      /* pass rate for this filter in this pos */
		/*
		 * pass = pass rate of all filters before this one 
		 */
		info = &fdata->fd_filters[pmElt(perm, i)];
		c = info->fi_time_ns;
		n = info->fi_called;
		if (n < FSTATS_VALID_NUM) {
			c = FSTATS_UNKNOWN_COST;
			n = FSTATS_UNKNOWN_NUM;
		}

		totalcost += pass * c / n;  /* prev cumul pass * curr cost */
		/*
		 * lookup this permutation 
		 */
		/*
		 * XXX 
		 */
		if (indep) {
			/*
			 * pretend there's no context 
			 */
			fprob = fexec_lookup_prob(fdata, pmElt(perm, i), 0, NULL);
		} else {
			fprob = fexec_lookup_prob(fdata, pmElt(perm, i), i, pmArr(perm));
		}
		if (fprob) {
			if (fprob->num_exec < FSTATS_VALID_NUM) {
				p = FSTATS_UNKNOWN_PROB;
			} else {
				p = (float) fprob->num_pass / fprob->num_exec;
			}
		} else {
			p = FSTATS_UNKNOWN_PROB;
		}

		assert(p >= 0 && p <= 1.0);
		pass *= p;
		/*
		 * don't let it go to zero XXX 
		 */
		if (pass < SMALL_FRACTION) {
			pass = SMALL_FRACTION;
		}
	}

	*cost = totalcost;
	return 0;
}

int
fexec_estimate_cur_cost(filter_data_t * fdata, float *cost)
{
	int             err;

	err = fexec_estimate_cost(fdata, fdata->fd_perm, 1, 0, cost);
	return err;
}

int
fexec_estimate_remaining( filter_data_t * fdata, permutation_t * perm,
                          int offset, int indep, float *cost)
{
	int             i;
	filter_prob_t  *fprob;
	float           pass = 1;   /* cumul pass rate */
	float           totalcost = 0;  /* = utility */
	filter_info_t  *info;
	int             n;

	// XXX printf("estmate: offset %d \n", offset);
	/*
	 * NB: this assumes that filter_id_t and pelt_t are the same type XXX 
	 */
	assert(sizeof(pelt_t) == sizeof(filter_id_t));

	for (i = offset; i < pmLength(perm); i++) {
		float           c;      /* cost of this filter */
		float           p;      /* pass rate for this filter in this pos */
		/*
		 * pass = pass rate of all filters before this one 
		 */
		info = &fdata->fd_filters[pmElt(perm, i)];
		c = info->fi_time_ns;
		n = info->fi_called;
		if (n < FSTATS_VALID_NUM) {
			c = FSTATS_UNKNOWN_COST;
			n = FSTATS_UNKNOWN_NUM;
		}

		totalcost += pass * c / n;  /* prev cumul pass * curr cost */
		/*
		 * lookup this permutation 
		 */
		/*
		 * XXX 
		 */
		if (indep) {
			/*
			 * pretend there's no context 
			 */
			fprob = fexec_lookup_prob(fdata, pmElt(perm, i), 0, NULL);
		} else {
			fprob = fexec_lookup_prob(fdata, pmElt(perm, i), i, pmArr(perm));
		}
		if (fprob) {
			if (fprob->num_exec < FSTATS_VALID_NUM) {
				p = FSTATS_UNKNOWN_PROB;
			} else {
				p = (float) fprob->num_pass / fprob->num_exec;
			}
		} else {
			p = FSTATS_UNKNOWN_PROB;
		}

		assert(p >= 0 && p <= 1.0);
		pass *= p;
		/*
		 * don't let it go to zero XXX 
		 */
		if (pass < SMALL_FRACTION) {
			pass = SMALL_FRACTION;
		}
	}

	// XXX printf("remain cost %f \n", totalcost);
	*cost = totalcost;
	return 0;
}

/*
 * evaluate a permutation in the context of the currently available data, and 
 * return a utility value (higher is better).  the function value is non-zero 
 * if not enough data (but the utility should then be set to an upper bound) 
 */
int
fexec_evaluate(filter_data_t * fdata, permutation_t * perm, int gen,
               int *utility)
{
	int             err;
	float           totalcost = 0;  /* = utility */

	err = fexec_compute_cost(fdata, perm, gen, 0, &totalcost);
	if (err == 0) {
		*utility = -totalcost;
	}
#ifdef VERBOSE
	{
		char            buf[BUFSIZ];
		// printf("fexec_evaluate: %s = %d\n", pmPrint(perm, buf, BUFSIZ),
		// *utility);
		printf("fexec_evaluate: ");
		fexec_print_cost(fdata, perm);
		printf(" cost=%s\n", format_number(buf, totalcost));
	}
#endif
	return err;
}


float
fexec_get_prate(filter_data_t *fdata)
{
	float           avg;
	if (fdata == NULL) {
		return (1.0);
	}
	avg = 1.0/fdata->fd_avg_wall;
	return(avg);
}



/*
 * same as fexec_evaluate, but assume filters are independent 
 */
int
fexec_evaluate_indep(filter_data_t * fdata, permutation_t * perm, int gen,
                     int *utility)
{
	int             err;
	float           totalcost = 0;  /* = utility */

	err = fexec_compute_cost(fdata, perm, gen, 1, &totalcost);
	if (err == 0) {
		*utility = -totalcost;
	}
#ifdef VERBOSE
	{
		char            buf[BUFSIZ];
		printf("fexec_evaluate_indep: %s = %d\n", pmPrint(perm, buf, BUFSIZ),
		       *utility);
		printf("fexec_evaluate_indep: ");
		fexec_print_cost(fdata, perm);
		printf(" cost=%s\n", format_number(buf, totalcost));
	}
#endif
	return err;
}


/*
 * not used? 
 */
/*
 * int 
 */
/*
 * fexec_single(filter_data_t *fdata, int fid, int *utility) { 
 */
/*
 * filter_info_t *info; 
 */
/*
 * float c, n, p; 
 */
/*
 * float wc; 
 */
/*
 * int err = 0; 
 */

/*
 * info = &fdata->fd_filters[fid]; 
 */
/*
 * n = info->fi_called; 
 */
/*
 * if(!n) n = 1; /\* avoid div 0 *\/ 
 */
/*
 * c = info->fi_time_ns / n; 
 */
/*
 * p = 1.0 - (float)info->fi_drop / n; 
 */

/*
 * wc = p * c; 
 */

/*
 *utility = (INT_MAX>>4) - wc; */
/*
 * assert(*utility >= 0); 
 */
/*
 * return err; 
 */
/*
 * } 
 */




/*
 * debug function 
 */
void
fexec_print_cost(const filter_data_t * fdata, const permutation_t * perm)
{
	int             i;
	const filter_info_t *info;
	float           c,
	p;
	int             n;
	char            buf[BUFSIZ];

	printf("[");
	for (i = 0; i < pmLength(perm); i++) {
		info = &fdata->fd_filters[pmElt(perm, i)];
		c = info->fi_time_ns;
		n = info->fi_called;
		if (!n)
			n = 1;              /* avoid div 0 */
		p = info->fi_called - info->fi_drop;
		printf(" %d=c%s,p%.0f/%d", pmElt(perm, i), format_number(buf, c / n),
		       p, n);
	}
	printf(" ]");
}


/*
 * only use this many entries. must be less than STAT_WINDOW 
 */
#define STAT_WINDOW_USED 1


void
fstat_add_obj_info(filter_data_t * fdata, int pass, rtime_t time_ns)
{
	float           sanity;

	sanity = time_ns;
	assert(sanity >= 0);

	fdata->obj_ns[fdata->obj_ns_pos] = time_ns;
	fdata->obj_ns_pos++;
	if (fdata->obj_ns_pos > STAT_WINDOW_USED) {
		fdata->obj_ns_pos = 0;
	}
	if (fdata->obj_ns_valid < STAT_WINDOW_USED) {
		fdata->obj_ns_valid++;
	}
}

char *
fstat_sprint(char *buf, const filter_data_t * fdata)
{
	int             i;
	int             pos =
	    ((STAT_WINDOW_USED + fdata->obj_ns_pos - fdata->obj_ns_valid)
	     % STAT_WINDOW_USED);
	float           total = 0;
	char            buf2[BUFSIZ];

	for (i = 0; i < fdata->obj_ns_valid; i++) {
		total += fdata->obj_ns[pos];
		pos++;
		if (pos >= STAT_WINDOW_USED) {
			pos = 0;
		}
	}
	sprintf(buf, "%s %f %d",
	        format_number(buf2, total / fdata->obj_ns_valid / 1000000000),
	        total, fdata->obj_ns_valid);
	return buf;
}
