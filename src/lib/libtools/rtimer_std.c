
/*
 * provides resource usage measurement, in particular timer, functionality.
 * 2003 Rajiv Wickremesinghe
 * based on a similar version
 * 2001 Rajiv Wickremesinghe, Duke University
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "rtimer_std.h"

/*
 * warning: assumes appropriate locks are already held when calling these
 * functions 
 */


void
rt_std_init(rtimer_std_t * rt)
{
    /*
     * null 
     */
}


void
rt_std_start(rtimer_std_t * rt)
{
    if (getrusage(RUSAGE_SELF, &rt->ru1) != 0) {
        perror("getrusage");
        exit(1);
    }
}

void
rt_std_stop(rtimer_std_t * rt)
{
    if (getrusage(RUSAGE_SELF, &rt->ru2) != 0) {
        perror("getrusage");
        exit(1);
    }
}



u_int64_t
rt_std_nanos(rtimer_std_t * rt)
{
    u_int64_t       nanos = 0;
    static const u_int64_t ns_s = 1000000000;
    static const u_int64_t us_s = 1000;

    /*
     * usr time 
     */
    nanos +=
        (u_int64_t) (rt->ru2.ru_utime.tv_sec -
                     rt->ru1.ru_utime.tv_sec) * ns_s;
    nanos +=
        (u_int64_t) (rt->ru2.ru_utime.tv_usec -
                     rt->ru1.ru_utime.tv_usec) * us_s;
    /*
     * sys time 
     */
    nanos +=
        (u_int64_t) (rt->ru2.ru_stime.tv_sec -
                     rt->ru1.ru_stime.tv_sec) * ns_s;
    nanos +=
        (u_int64_t) (rt->ru2.ru_stime.tv_usec -
                     rt->ru1.ru_stime.tv_usec) * us_s;

    return nanos;
}
