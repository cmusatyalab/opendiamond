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
	int      rt_papi_initialized(void);


	/*
	 * public 
	 */

	/*
	 * should be called at the start of the program, BEFORE spawning any
	 * threads 
	 */
	int      rt_papi_global_init(void);

	void     rt_papi_init(rtimer_papi_t * rt);
	void     rt_papi_start(rtimer_papi_t * rt);
	void     rt_papi_stop(rtimer_papi_t * rt);
	rtime_t  rt_papi_nanos(rtimer_papi_t * rt);

#ifdef __cplusplus
}
#endif
#endif
