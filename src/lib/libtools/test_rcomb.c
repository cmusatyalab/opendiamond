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


#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "rcomb.h"

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
