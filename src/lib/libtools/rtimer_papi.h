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


#ifndef RTIMER_PAPI_H
#define RTIMER_PAPI_H

#include "rtimer.h"

#ifdef __cplusplus
extern          "C"
{
#endif

	/*
	 * protected 
	 */
	extern int      rt_papi_initialized();


	/*
	 * public 
	 */

	/*
	 * should be called at the start of the program, BEFORE spawning any
	 * threads 
	 */
	extern int      rt_papi_global_init();

	extern void     rt_papi_init(rtimer_papi_t * rt);
	extern void     rt_papi_start(rtimer_papi_t * rt);
	extern void     rt_papi_stop(rtimer_papi_t * rt);
	extern rtime_t  rt_papi_nanos(rtimer_papi_t * rt);

#ifdef __cplusplus
}
#endif
#endif
