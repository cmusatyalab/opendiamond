#ifndef RTIMER_STD_H
#define RTIMER_STD_H

#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>


/* struct rtimer_t { */
/*   struct rusage ru1, ru2; */
/* }; */


#ifdef __cplusplus
extern "C" {
#endif

extern void        rt_std_init(rtimer_std_t *rt);
extern void        rt_std_start(rtimer_std_t *rt);
extern void        rt_std_stop(rtimer_std_t *rt);
extern rtime_t     rt_std_nanos(rtimer_std_t *rt);

#ifdef __cplusplus
}
#endif


#endif
