/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 3
 *
 *  Copyright (c) 2002-2005 Intel Corporation
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#include <stdio.h>
#include <math.h>
#include "rtimer.h"


int
main(int argc, char **argv)
{

	rtimer_t        rt;
	int             i;
	double          x;

	while (1) {
		rt_start(&rt);
		x = 1.14;
		for (i = 0; i < 1000000; i++) {
			x *= 1.2 + sqrt(i);
		}
		rt_stop(&rt);
		printf("%f\n", rt_nanos(&rt));
	}

	return 0;
}
