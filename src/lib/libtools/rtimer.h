#ifndef _RTIMER_H_
#define _RTIMER_H_

#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>


typedef struct rtimer {
  struct rusage ru1, ru2;
} rtimer_t;

typedef u_int64_t  rtime_t;

extern void        rt_init(rtimer_t *rt);
extern void        rt_start(rtimer_t *rt);
extern void        rt_stop(rtimer_t *rt);
extern rtime_t     rt_nanos(rtimer_t *rt);

extern double      rt_time2secs(rtime_t t);


#endif /* _RTIMER_H_ */
