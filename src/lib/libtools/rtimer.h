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

/* #ifdef RTIMER_STD */
/* #include "rtimer_std.h" */
/* #endif */

/* #ifdef RTIMER_PAPI */
/* #include "rtimer_papi.h" */
/* #endif */

typedef struct rtimer_std_t {
  struct rusage ru1, ru2;
} rtimer_std_t;

typedef struct rtimer_papi_t {
  int valid;
  int EventSet;
  u_int64_t cycles;
} rtimer_papi_t;

struct rtimer_t {
  union {
    struct rtimer_std_t   std;
    struct rtimer_papi_t  papi;
  };
};


#ifdef __cplusplus
extern "C" {
#endif

struct rtimer_t;
typedef struct rtimer_t rtimer_t;

typedef u_int64_t  rtime_t;

typedef enum {
  RTIMER_STD=1,
  RTIMER_PAPI
} rtimer_mode_t;

void rtimer_system_init(rtimer_mode_t mode);

extern pthread_attr_t *pattr_default;
#define PATTR_DEFAULT pattr_default

extern void        rt_init(rtimer_t *rt);
extern void        rt_start(rtimer_t *rt);
extern void        rt_stop(rtimer_t *rt);
extern rtime_t     rt_nanos(rtimer_t *rt);

extern double      rt_time2secs(rtime_t t);

#ifdef __cplusplus
}
#endif

//#include "rtimer_common.h"


#endif /* _RTIMER_H_ */
