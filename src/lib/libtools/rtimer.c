
#include "rtimer.h"
#include "rtimer_std.h"
#include "rtimer_papi.h"

typedef enum {
  RTIMER_STD=0,
  RTIMER_PAPI
} rtimer_mode_t;

static rtimer_mode_t rtimer_mode = RTIMER_STD;



void
rt_init(rtimer_t *rt) {
  switch(rtimer_mode) {
  case RTIMER_STD:
    rt_std_init((void*)rt);
    break;
  case RTIMER_PAPI:
    rt_papi_init((void*)rt);
    break;
  }
}


void
rt_start(rtimer_t *rt) {
  switch(rtimer_mode) {
  case RTIMER_STD:
    rt_std_start((void*)rt);
    break;
  case RTIMER_PAPI:
    rt_papi_start((void*)rt);
    break;
  }
}


void
rt_stop(rtimer_t *rt) {
  switch(rtimer_mode) {
  case RTIMER_STD:
    rt_std_stop((void*)rt);
    break;
  case RTIMER_PAPI:
    rt_papi_stop((void*)rt);
    break;
  }
}


rtime_t
rt_nanos(rtimer_t *rt) {
  switch(rtimer_mode) {
  case RTIMER_STD:
    return rt_std_nanos((void*)rt);
    break;
  case RTIMER_PAPI:
    return rt_papi_nanos((void*)rt);
    break;
  }
  return 0;			/* not reached */
}


double
rt_time2secs(rtime_t t) {
  double secs = t;
  secs /= 1000000000.0;
  return secs;
}


