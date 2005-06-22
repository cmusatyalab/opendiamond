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


#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "rcomb.h"


static char const cvsid[] = "$Header$";


int
evaluate(void *context, permutation_t * perm, int *score)
{
	int             i,
	n;

	if ((random() & 3) >= 3) {
		return RC_ERR_NODATA;
	}

	n = perm->length;
	*score = 100;
	for (i = 1; i < n; i++) {
		if (pmElt(perm, i - 1) < pmElt(perm, i)) {
			*score -= pmElt(perm, i) - pmElt(perm, i - 1);
		}
	}
	return 0;
}


int
main(int argc, char **argv)
{
	hc_state_t      hc;
	permutation_t  *start;
	int             n = 8;
	int             i;
	partial_order_t *po;
	int             err = 0;
	char            buf[BUFSIZ];

	po = poNew(n);
	// poSetOrder(po, 0, 1, PO_LT);

	start = pmNew(n);
	for (i = 0; i < n; i++) {
		pmSetElt(start, i, i);
	}
	hill_climb_init(&hc, start);

	while (err != RC_ERR_COMPLETE) {
		err = hill_climb_step(&hc, po, evaluate, NULL);
		if (err) {
			printf("got error: %d\n", err);
		} else {
			printf("selected: %s\n",
			       pmPrint(hill_climb_next(&hc), buf, BUFSIZ));
		}
	}
	hill_climb_cleanup(&hc);
	poDelete(po);
	return 0;
}
