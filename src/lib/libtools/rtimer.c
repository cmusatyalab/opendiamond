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

#include <stdio.h>
#include "rtimer.h"
#include "rtimer_std.h"
#include "rtimer_papi.h"


static char const cvsid[] =
    "$Header$";

pthread_attr_t *pattr_default = NULL;
static rtimer_mode_t rtimer_mode = 0;

/*
 * should be called at startup 
 */
void
rtimer_system_init(rtimer_mode_t mode)
{
	switch (mode) {
#ifdef	PAPI_SUPPORT
	case RTIMER_PAPI:
		if (rt_papi_global_init() == 0) {
			rtimer_mode = RTIMER_PAPI;
		} else {
			fprintf(stderr,
				"rtimer: PAPI error, reverting to STD\n");
		}
		break;
#endif

	case RTIMER_STD:	/* no global init for std */
	default:
		rtimer_mode = RTIMER_STD;
		break;
	}
}


void
rt_init(rtimer_t * rt)
{

	/*
	 * in case the system was not initialized, revert to std behaviour
	 * (which doesn't need a global init) 
	 */
	if (!rtimer_mode) {
		rtimer_mode = RTIMER_STD;
	}

	switch (rtimer_mode) {
#ifdef	PAPI_SUPPORT
	case RTIMER_PAPI:
		rt_papi_init((void *) rt);
		break;
#endif

	case RTIMER_STD:
	default:
		rt_std_init((void *) rt);
		break;
	}
}


void
rt_start(rtimer_t * rt)
{
	switch (rtimer_mode) {
#ifdef	PAPI_SUPPORT
	case RTIMER_PAPI:
		rt_papi_start((void *) rt);
		break;
#endif

	case RTIMER_STD:
	default:
		rt_std_start((void *) rt);
		break;
	}
}


void
rt_stop(rtimer_t * rt)
{
	switch (rtimer_mode) {
#ifdef	PAPI_SUPPORT
	case RTIMER_PAPI:
		rt_papi_stop((void *) rt);
		break;
#endif

	case RTIMER_STD:
	default:
		rt_std_stop((void *) rt);
		break;
	}
}


rtime_t
rt_nanos(rtimer_t * rt)
{
	switch (rtimer_mode) {
#ifdef	PAPI_SUPPORT
	case RTIMER_PAPI:
		return rt_papi_nanos((void *) rt);
		break;
#endif

	case RTIMER_STD:
	default:
		return rt_std_nanos((void *) rt);
		break;
	}
	return 0;		/* not reached */
}


double
rt_time2secs(rtime_t t)
{
	double          secs = t;
	secs /= 1000000000.0;
	return secs;
}
