
#ifndef RTIMER_PAPI_H
#define RTIMER_PAPI_H

/* struct rtimer_t { */
/*   int valid; */
/*   int EventSet; */
/*   u_int64_t cycles; */
/* }; */


#ifdef __cplusplus
extern "C" {
#endif

extern void        rt_papi_init(rtimer_papi_t *rt);
extern void        rt_papi_start(rtimer_papi_t *rt);
extern void        rt_papi_stop(rtimer_papi_t *rt);
extern rtime_t     rt_papi_nanos(rtimer_papi_t *rt);

#ifdef __cplusplus
}
#endif

#endif
