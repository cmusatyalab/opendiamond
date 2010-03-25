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

#include <stdio.h>
#include <stdlib.h>
#include "rtimer.h"


void
rt_start(rtimer_t * rt)
{
	if (getrusage(RUSAGE_SELF, &rt->ru1) != 0) {
		perror("getrusage");
		exit(1);
	}
}


void
rt_stop(rtimer_t * rt)
{
	if (getrusage(RUSAGE_SELF, &rt->ru2) != 0) {
		perror("getrusage");
		exit(1);
	}
}


rtime_t
rt_nanos(rtimer_t * rt)
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


double
rt_time2secs(rtime_t t)
{
	double          secs = t;
	secs /= 1000000000.0;
	return secs;
}
