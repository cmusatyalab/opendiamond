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

typedef struct
{
	int             valid;
	int             size;
	int             capacity;
	pelt_t          elements[0];
}
permutation_t;


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

typedef struct partial_order_t
{
	int             dim;
	char            data[0];
}
partial_order_t;


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

#endif                          /* RCOMB_H */
