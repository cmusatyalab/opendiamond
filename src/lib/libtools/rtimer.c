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


#include <stdio.h>
#include "rtimer.h"
#include "rtimer_std.h"
#include "rtimer_papi.h"

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
            fprintf(stderr, "rtimer: PAPI error, reverting to STD\n");
        }
        break;
#endif
    case RTIMER_STD:           /* no global init for std */
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
    return 0;                   /* not reached */
}


double
rt_time2secs(rtime_t t)
{
    double          secs = t;
    secs /= 1000000000.0;
    return secs;
}
