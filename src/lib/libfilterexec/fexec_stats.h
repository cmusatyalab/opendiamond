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
