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


/*
 * combinatorics support 
 */

/*
 * Rajiv Wickremesinghe 5/2003 
 */


#ifndef RCOMB_H
#define RCOMB_H


/*
 * ---------------------------------------------------------------------- 
 */

typedef u_int8_t pelt_t;

typedef struct {
    int             valid;
    int             size;
    int             capacity;
    pelt_t          elements[0];
} permutation_t;


permutation_t  *pmNew(int length);
void            pmDelete(permutation_t * ptr);

void            pmCopy(permutation_t * copy, const permutation_t * ptr);
permutation_t  *pmDup(const permutation_t * ptr);

int             pmLength(const permutation_t * ptr);
void            pmSwap(permutation_t * ptr, int i, int j);
pelt_t          pmElt(const permutation_t * pm, int i);
void            pmSetElt(permutation_t * pm, int i, pelt_t val);
const pelt_t   *pmArr(const permutation_t * pm);

int             pmEqual(const permutation_t *, const permutation_t *);

char           *pmPrint(const permutation_t * pm, char *buf, int bufsiz);


/*
 * ---------------------------------------------------------------------- 
 */

typedef enum {
    PO_EQ = 0,
    PO_LT = -1,
    PO_GT = 1,
    PO_INCOMPARABLE = 3
} po_relation_t;

typedef struct partial_order_t {
    int             dim;
    char            data[0];
} partial_order_t;


partial_order_t *poNew(int n);
void            poDelete(partial_order_t * po);

/*
 * NB this only sets the pairwise--(u,v) and (v,u)--comparison; no
 * transitivity 
 */
void            poSetOrder(partial_order_t * po, int u, int v,
                           po_relation_t rel);

/*
 * add the closure 
 */
void            poClosure(partial_order_t * po);

int             poIncomparable(const partial_order_t * po, int u, int v);

void            poPrint(partial_order_t * po);

int      poGet(const partial_order_t * po, int u, int v);

/*
 * ---------------------------------------------------------------------- 
 */

typedef int     (*evaluation_func_t) (const void *context,
                                      permutation_t * seq, int gen,
                                      int *score);


/*
 * error codes 
 */
enum {
    RC_ERR_NONE = 0,
    RC_ERR_COMPLETE = 1,
    RC_ERR_NODATA,
    RC_ERR_CONTINUE
};


/*
 * ---------------------------------------------------------------------- 
 */

typedef struct hc_state_t {
    permutation_t  *global_best;
    permutation_t  *best_seq,
                   *next_seq;
    int             n,
                    i,
                    j;
    int             improved;
    int             generation;
    int             best_err_count;
} hc_state_t;


void            hill_climb_init(hc_state_t * ptr, const permutation_t * start,
                                const partial_order_t * po);
void            hill_climb_cleanup(hc_state_t * ptr);

int             hill_climb_step(hc_state_t * hc, const partial_order_t * po,
                                evaluation_func_t func, void *context);

const permutation_t *hill_climb_result(hc_state_t * hc);
const permutation_t *hill_climb_next(hc_state_t * hc);

void            randomize_permutation(permutation_t * perm,
                                      const partial_order_t * po);

/*
 * ---------------------------------------------------------------------- 
 */


typedef enum {
    RC_BFS_INIT,
    RC_BFS_VISIT,
    RC_BFS_EXPAND,
    RC_BFS_DONE
} bf_dfa_t;

struct heap_t;

typedef struct bf_state_t {
    bf_dfa_t        state;      /* state machine state */
    int             n;          /* size of perm */
    int             i;          /* initialization */
    int             j;          /* inner loop state */
    int             improved;
    const partial_order_t *po;
    struct heap_t  *pq;

    /*
     * common state 
     */
    permutation_t  *best_seq;
    permutation_t  *next_seq;

    evaluation_func_t evfunc;
    const void     *evcontext;
    int             generation; /* current generation number (number of times 
                                 * alg completed) */
} bf_state_t;


void            best_first_init(bf_state_t * ptr, int n,
                                const partial_order_t * po,
                                evaluation_func_t func, const void *context);
void            best_first_cleanup(bf_state_t * ptr);

int             best_first_step(bf_state_t * hc);

const permutation_t *best_first_result(bf_state_t * hc);
const permutation_t *best_first_next(bf_state_t * hc);


/*
 * ---------------------------------------------------------------------- 
 */

typedef struct indep_state_t {

    /*
     * common state 
     */
    permutation_t  *best_seq;
    permutation_t  *next_seq;

    evaluation_func_t evfunc;
    const void     *evcontext;
    int             generation; /* current generation number (number of times 
                                 * alg completed) */

} indep_state_t;

void            indep_init(indep_state_t *, int n, const partial_order_t * po,
                           evaluation_func_t func, const void *context);
void            indep_cleanup(indep_state_t *);

int             indep_step(indep_state_t *);

const permutation_t *indep_result(indep_state_t *);
const permutation_t *indep_next(indep_state_t *);


/*
 * ---------------------------------------------------------------------- 
 */

char           *format_number(char *buf, double val);





#endif                          /* RCOMB_H */
