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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "rcomb.h"


// #define VERBOSE 1

/*
 * ---------------------------------------------------------------------- 
 */

#define PM_VALID_MAGIC 0xaf95
#define PM_CHECK_VALID(x) (assert((x)->valid == PM_VALID_MAGIC))

permutation_t  *
pmNew(int n)
{
	permutation_t  *ptr;

	assert(n >= 0);
	ptr = (permutation_t *) malloc(sizeof(permutation_t) +
				       sizeof(pelt_t) * n);
	assert(ptr);
	ptr->size = 0;
	ptr->capacity = n;
	ptr->valid = PM_VALID_MAGIC;

	return ptr;
}

void
pmDelete(permutation_t * ptr)
{
	if (ptr) {
		PM_CHECK_VALID(ptr);
		ptr->valid = 0;
		free(ptr);
	}
}


void
pmCopy(permutation_t * copy, const permutation_t * ptr)
{
	int             i;

	PM_CHECK_VALID(ptr);
	PM_CHECK_VALID(copy);
	assert(copy->capacity >= ptr->size);
	for (i = 0; i < ptr->size; i++) {
		copy->elements[i] = ptr->elements[i];
	}
	copy->size = ptr->size;
}


static void
pmCopyAll(permutation_t * copy, const permutation_t * ptr)
{
	int             i;

	PM_CHECK_VALID(ptr);
	PM_CHECK_VALID(copy);
	assert(copy->capacity >= ptr->capacity);
	for (i = 0; i < ptr->capacity; i++) {
		copy->elements[i] = ptr->elements[i];
	}
	copy->size = ptr->size;
}

permutation_t  *
pmDup(const permutation_t * ptr)
{
	permutation_t  *copy;
	int             i;

	PM_CHECK_VALID(ptr);
	copy = pmNew(ptr->capacity);
	/*
	 * copy all elements, not just valid ones (overloaded semantic XXX) 
	 */
	for (i = 0; i < ptr->capacity; i++) {
		copy->elements[i] = ptr->elements[i];
	}
	copy->size = ptr->size;
	return copy;
}

pelt_t
pmElt(const permutation_t * pm, int i)
{
	PM_CHECK_VALID(pm);
	assert(i < pm->capacity);
	// assert(i < pm->size);
	return pm->elements[i];
}

void
pmSetElt(permutation_t * pm, int i, pelt_t val)
{
	PM_CHECK_VALID(pm);
	assert(i < pm->capacity);
	if (i >= pm->size) {
		pm->size = i + 1;
	}
	pm->elements[i] = val;
}

static int
pmSize(permutation_t * pm)
{
	return pm->size;
}

static void
pmSetSize(permutation_t * pm, int n)
{
	assert(n <= pm->capacity);
	pm->size = n;
}

static void
pmIdentity(permutation_t * pm)
{
	int             i;

	for (i = 0; i < pm->capacity; i++) {
		pm->elements[i] = i;
	}
	pm->size = pm->capacity;
}

const pelt_t   *
pmArr(const permutation_t * pm)
{
	PM_CHECK_VALID(pm);
	return pm->elements;
}

int
pmLength(const permutation_t * pm)
{
	PM_CHECK_VALID(pm);
	return pm->size;
}

void
pmSwap(permutation_t * pm, int i, int j)
{
	pelt_t          tmp;

	PM_CHECK_VALID(pm);
	// assert(i < pm->size);
	// assert(j < pm->size);
	tmp = pm->elements[i];
	pm->elements[i] = pm->elements[j];
	pm->elements[j] = tmp;
}

int
pmEqual(const permutation_t * pm1, const permutation_t * pm2)
{
	int             i;

	if (pm1->size != pm2->size) {
		return 0;
	}
	for (i = 0; i < pm1->size; i++) {
		if (pm1->elements[i] != pm2->elements[i]) {
			return 0;
		}
	}
	return 1;
}


char           *
pmPrint(const permutation_t * pm, char *buf, int bufsiz)
{
	int             i;
	char            buf2[BUFSIZ];

	if (!pm) {
		sprintf(buf, "[<null>]");
		return buf;
	}

	sprintf(buf, "[");
	for (i = 0; i < pm->size; i++) {
		sprintf(buf2, "%s%d", (i ? " " : ""), pm->elements[i]);
		if (strlen(buf) + strlen(buf2) > bufsiz) {
			break;
		}
		strcat(buf, buf2);
	}
	strcat(buf, "]");
	return buf;
}

/*
 * ---------------------------------------------------------------------- 
 */
/*
 * poset functions 
 */

// static int poGet(const partial_order_t * po, int u, int v);
int             poGet(const partial_order_t * po, int u, int v);


partial_order_t *
poNew(int n)
{
	partial_order_t *po;

	po = (partial_order_t *) malloc(sizeof(partial_order_t) +
					n * n * sizeof(char));
	assert(po);
	po->dim = n;
	memset(po->data, PO_INCOMPARABLE, n * n * sizeof(char));

	return po;
}

void
poDelete(partial_order_t * po)
{
	if (po) {
		free(po);
	}
}

static int
poIsMin(const partial_order_t * po, int u)
{
	int             i;
	for (i = 0; i < po->dim; i++) {
		if (poGet(po, u, i) == PO_GT) {
			return 0;
		}
	}
	return 1;
}

void
poPrint(partial_order_t * po)
{
	int             i,
	                j;

	printf("   ");
	for (i = 0; i < po->dim; i++) {
		printf(" %1x", i);
	}
	printf("\n");

	for (i = 0; i < po->dim; i++) {
		printf(" %2d", i);
		for (j = 0; j < po->dim; j++) {
			char            c = ' ';
			switch (poGet(po, i, j)) {
			case PO_EQ:
				c = '=';
				break;
			case PO_LT:
				c = '<';
				break;
			case PO_GT:
				c = '>';
				break;
			default:
				c = '?';
			}
			printf(" %c", c);
		}
		printf("\n");
	}
}

static          po_relation_t
poInverse(po_relation_t rel)
{
	return (rel == PO_INCOMPARABLE ? rel : -rel);
}


void
poSetOrder(partial_order_t * po, int u, int v, po_relation_t rel)
{
	assert(u < po->dim);
	assert(v < po->dim);
	po->data[u * po->dim + v] = rel;
	po->data[v * po->dim + u] = poInverse(rel);
}

// static int
int
poGet(const partial_order_t * po, int u, int v)
{
	assert(u < po->dim);
	assert(v < po->dim);
	return po->data[u * po->dim + v];
}

void
poClosure(partial_order_t * po)
{
	int             i,
	                j,
	                k;
	int             n = po->dim;

	/*
	 * Warshall's alg 
	 */
	for (k = 0; k < n; k++) {
		for (i = 0; i < n; i++) {
			for (j = 0; j < n; j++) {
				if (poIncomparable(po, i, j)) {
					if (poGet(po, i, k) ==
					    poGet(po, k, j)) {
						poSetOrder(po, i, j,
							   poGet(po, i, k));
					}
				}
			}
		}
	}
}

int
poIncomparable(const partial_order_t * po, int u, int v)
{
	char            cmp;

	assert(u < po->dim);
	assert(v < po->dim);
	cmp = po->data[u * po->dim + v];

	/*
	 * if(cmp == PO_INCOMPARABLE) { 
	 */
	/*
	 * printf("INCMP: %d %d\n", u, v); 
	 */
	/*
	 * } 
	 */

	return (cmp == PO_INCOMPARABLE);
}

static int
poComparable(const partial_order_t * po, int u, int v)
{
	char            cmp;

	assert(u < po->dim);
	assert(v < po->dim);
	cmp = po->data[u * po->dim + v];

	return (cmp != PO_INCOMPARABLE);
}
