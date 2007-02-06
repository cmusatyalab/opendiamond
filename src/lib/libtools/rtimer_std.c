/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 2
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
 * provides resource usage measurement, in particular timer, functionality.
 * 2003 Rajiv Wickremesinghe
 * based on a similar version
 * 2001 Rajiv Wickremesinghe, Duke University
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "rtimer_std.h"


static char const cvsid[] =
    "$Header$";

/*
 * warning: assumes appropriate locks are already held when calling these
 * functions 
 */


void
rt_std_init(rtimer_std_t * rt)
{
	/*
	 * null 
	 */
}


void
rt_std_start(rtimer_std_t * rt)
{
	if (getrusage(RUSAGE_SELF, &rt->ru1) != 0) {
		perror("getrusage");
		exit(1);
	}
}

void
rt_std_stop(rtimer_std_t * rt)
{
	if (getrusage(RUSAGE_SELF, &rt->ru2) != 0) {
		perror("getrusage");
		exit(1);
	}
}



u_int64_t
rt_std_nanos(rtimer_std_t * rt)
{
	u_int64_t       nanos = 0;
	static const u_int64_t ns_s = 1000000000;
	static const u_int64_t us_s = 1000;

	/*
	 * usr time 
	 */
	nanos +=
	    (u_int64_t) (rt->ru2.ru_utime.tv_sec -
			 rt->ru1.ru_utime.tv_sec) * ns_s;
	nanos +=
	    (u_int64_t) (rt->ru2.ru_utime.tv_usec -
			 rt->ru1.ru_utime.tv_usec) * us_s;
	/*
	 * sys time 
	 */
	nanos +=
	    (u_int64_t) (rt->ru2.ru_stime.tv_sec -
			 rt->ru1.ru_stime.tv_sec) * ns_s;
	nanos +=
	    (u_int64_t) (rt->ru2.ru_stime.tv_usec -
			 rt->ru1.ru_stime.tv_usec) * us_s;

	return nanos;
}
