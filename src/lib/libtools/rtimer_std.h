/*
 * 	OpenDiamond 2.0
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

#ifndef RTIMER_STD_H
#define RTIMER_STD_H

#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include "rtimer.h"


#ifdef __cplusplus
extern          "C"
{
#endif

	extern void     rt_std_init(rtimer_std_t * rt);
	extern void     rt_std_start(rtimer_std_t * rt);
	extern void     rt_std_stop(rtimer_std_t * rt);
	extern rtime_t  rt_std_nanos(rtimer_std_t * rt);

#ifdef __cplusplus
}
#endif
#endif
