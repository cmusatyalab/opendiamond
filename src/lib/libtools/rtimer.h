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

#ifndef _RTIMER_H_
#define _RTIMER_H_

/*
 * provides resource usage measurement, in particular timer, functionality.
 * 2003 Rajiv Wickremesinghe
 * based on a similar version
 * 2001 Rajiv Wickremesinghe, Duke University
 */


#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>

/*
 * #ifdef RTIMER_STD 
 */
/*
 * #include "rtimer_std.h" 
 */
/*
 * #endif 
 */

/*
 * #ifdef RTIMER_PAPI 
 */
/*
 * #include "rtimer_papi.h" 
 */
/*
 * #endif 
 */

typedef struct rtimer_std_t
{
	struct rusage   ru1,
				ru2;
}
rtimer_std_t;

typedef struct rtimer_papi_t
{
	int             valid;
	int             EventSet;
	u_int64_t       cycles;
}
rtimer_papi_t;

struct rtimer_t
{
	union {
		struct rtimer_std_t std;
		struct rtimer_papi_t papi;
	};
};


#ifdef __cplusplus
extern          "C"
{
#endif

	struct rtimer_t;
	typedef struct rtimer_t rtimer_t;

	typedef u_int64_t rtime_t;

	typedef enum {
	    RTIMER_STD = 1,
	    RTIMER_PAPI
	} rtimer_mode_t;

	void            rtimer_system_init(rtimer_mode_t mode);
	void     rt_init(rtimer_t * rt);
	void     rt_start(rtimer_t * rt);
	void     rt_stop(rtimer_t * rt);
	rtime_t  rt_nanos(rtimer_t * rt);

	double   rt_time2secs(rtime_t t);

#ifdef __cplusplus
}
#endif

#endif                          /* _RTIMER_H_ */
