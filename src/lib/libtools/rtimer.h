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

typedef struct rtimer_std_t {
    struct rusage   ru1,
                    ru2;
} rtimer_std_t;

typedef struct rtimer_papi_t {
    int             valid;
    int             EventSet;
    u_int64_t       cycles;
} rtimer_papi_t;

struct rtimer_t {
    union {
        struct rtimer_std_t std;
        struct rtimer_papi_t papi;
    };
};


#ifdef __cplusplus
extern          "C" {
#endif

    struct rtimer_t;
    typedef struct rtimer_t rtimer_t;

    typedef u_int64_t rtime_t;

    typedef enum {
        RTIMER_STD = 1,
        RTIMER_PAPI
    } rtimer_mode_t;

    void            rtimer_system_init(rtimer_mode_t mode);

    extern pthread_attr_t *pattr_default;
#define PATTR_DEFAULT pattr_default

    extern void     rt_init(rtimer_t * rt);
    extern void     rt_start(rtimer_t * rt);
    extern void     rt_stop(rtimer_t * rt);
    extern rtime_t  rt_nanos(rtimer_t * rt);

    extern double   rt_time2secs(rtime_t t);

#ifdef __cplusplus
}
#endif
// #include "rtimer_common.h"
#endif                          /* _RTIMER_H_ */
