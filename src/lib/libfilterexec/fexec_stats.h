/*
 * 	Diamond
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


#define	FSTATS_VALID_NUM		(5)
#define	FSTATS_UNKNOWN_COST		(100000000)
#define	FSTATS_UNKNOWN_PROB		(1.0)
#define	FSTATS_UNKNOWN_NUM		(1)



/*
 * evaluate a permutation 
 */
int             fexec_evaluate(filter_data_t * fdata, permutation_t * perm,
                               int gen, int *utility);
int             fexec_evaluate_indep(filter_data_t * fdata,
                                     permutation_t * perm, int gen,
                                     int *utility);

int 		fexec_estimate_remaining( filter_data_t * fdata,
                                permutation_t * perm, int offset, int indep,
                                float *cost);


/*
 * evaluate a single filter 
 */
int             fexec_single(filter_data_t * fdata, int fid, int *utility);


/*
 * debug 
 */
void            fexec_print_cost(const filter_data_t * fdata,
                                 const permutation_t * perm);

void            fstat_add_obj_info(filter_data_t * fdata, int pass,
                                   rtime_t time_ns);
char           *fstat_sprint(char *buf, const filter_data_t * fdata);
