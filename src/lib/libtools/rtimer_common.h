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

#ifdef __cplusplus
extern          "C"
{
#endif

	struct rtimer_t;
	typedef struct rtimer_t rtimer_t;

	typedef u_int64_t rtime_t;

	extern void     rt_init(rtimer_t * rt);
	extern void     rt_start(rtimer_t * rt);
	extern void     rt_stop(rtimer_t * rt);
	extern rtime_t  rt_nanos(rtimer_t * rt);

	extern double   rt_time2secs(rtime_t t);



#ifdef __cplusplus
}
#endif
