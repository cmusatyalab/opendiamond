
#ifndef RTIMER_PAPI_H
#define RTIMER_PAPI_H

#include "rtimer.h"

#ifdef __cplusplus
extern "C" {
#endif

/* protected */
extern int         rt_papi_initialized();


/* public */

/* should be called at the start of the program, BEFORE spawning any threads */
extern int         rt_papi_global_init();

extern void        rt_papi_init(rtimer_papi_t *rt);
extern void        rt_papi_start(rtimer_papi_t *rt);
extern void        rt_papi_stop(rtimer_papi_t *rt);
extern rtime_t     rt_papi_nanos(rtimer_papi_t *rt);

#ifdef __cplusplus
}
#endif

#endif
