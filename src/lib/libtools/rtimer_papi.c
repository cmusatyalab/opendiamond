/*
 * provides resource usage measurement, in particular timer, functionality.
 * 2003 Rajiv Wickremesinghe
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "rtimer_papi.h"
/*
 * #include "rtimer_common.h" 
 */
#include "rtimer.h"

#include <pthread.h>
#include "papiStdEventDefs.h"
#include "papi.h"

/*
 * warning: assumes appropriate locks are already held when calling these
 * functions 
 */



#define RT_MAGIC 0xe251a
#define CHECK_VALID(rt) assert((rt)->valid == RT_MAGIC)

#define NUM_EVENTS 2
static int      Events[NUM_EVENTS] = { PAPI_TOT_INS, PAPI_TOT_CYC };

static const PAPI_hw_info_t *hwinfo = NULL;



static void
report_error(char *file, int line, char *msg, int err)
{
    fprintf(stderr, "ERROR %d: %s\n", err, msg);
}

/*
 * begin XXX move elsewhere 
 */

#include <stdarg.h>

static void
log_utility_message(char *fmt, ...)
{
    va_list         ap;
    va_list         new_ap;

    va_start(ap, fmt);
    va_copy(new_ap, ap);
    fprintf(stderr, "UTILITY ERROR:");
    fprintf(stderr, fmt, new_ap);
    /*
     * log_message(LOGT_UTLITY, level, fmt, new_ap); 
     */
    va_end(ap);
}

/*
 * end XXX move elsewhere 
 */

int
rt_papi_global_init()
{
    static int      inited = 0;
    int             err;
    extern pthread_attr_t *pattr_default;

    pattr_default = NULL;

    if (inited) {
        return 0;
    }

    if ((err = PAPI_library_init(PAPI_VER_CURRENT)) != PAPI_VER_CURRENT) {
        log_utility_message("PAPI_library_init: %d", err);
        return 1;
    }
    fprintf(stderr, "papi initialized OK\n");

    if ((hwinfo = PAPI_get_hardware_info()) == NULL) {
        report_error(__FILE__, __LINE__, "PAPI_get_hardware_info", 0);
        return 1;
    }
    fprintf(stderr, "papi mhz=%.2fMHz\n", hwinfo->mhz);


    /*
     * thread init 
     */

    err = PAPI_thread_init((unsigned long (*)(void)) (pthread_self), 0);
    if (err != PAPI_OK) {
        report_error(__FILE__, __LINE__, "PAPI_thread_init", err);
        return 1;
    }
    fprintf(stderr, "papi thread init OK\n");

    /*
     * pthread attr init 
     */

    pattr_default = (pthread_attr_t *) malloc(sizeof(pthread_attr_t));  /* not 
                                                                         * free'd 
                                                                         * XXX 
                                                                         */
    if (!pattr_default) {
        report_error(__FILE__, __LINE__, "malloc", 0);
        return 1;
    }

    pthread_attr_init(pattr_default);
#ifdef PTHREAD_CREATE_UNDETACHED
    pthread_attr_setdetachstate(pattr_default, PTHREAD_CREATE_UNDETACHED);
#endif
#ifdef PTHREAD_SCOPE_SYSTEM
    err = pthread_attr_setscope(pattr_default, PTHREAD_SCOPE_SYSTEM);
    if (err != 0) {
        report_error(__FILE__, __LINE__, "pthread_attr_setscope", err);
        return 1;
    }
#endif

    inited = 1;
    return 0;
}

int
rt_papi_initialized()
{
    return PAPI_initialized();
}

void
rt_papi_init(rtimer_papi_t * rt)
{
    int             err;

    if (!rt_papi_initialized()) {
        report_error(__FILE__, __LINE__, "PAPI not inited", 1);
    }

    if ((err = PAPI_create_eventset(&rt->EventSet)) != PAPI_OK) {
        report_error(__FILE__, __LINE__, "PAPI_create_eventset", err);
    }

    if ((err = PAPI_add_events(&rt->EventSet, Events, NUM_EVENTS)) != PAPI_OK) {
        report_error(__FILE__, __LINE__, "PAPI_add_events", err);
    }

    rt->valid = RT_MAGIC;
}


void
rt_papi_start(rtimer_papi_t * rt)
{
    int             err;

    CHECK_VALID(rt);

    if ((err = PAPI_start(rt->EventSet)) != PAPI_OK) {
        report_error(__FILE__, __LINE__, "PAPI_start", err);
    }
}

void
rt_papi_stop(rtimer_papi_t * rt)
{
    int             err;
    long_long       values[NUM_EVENTS];

    CHECK_VALID(rt);

    if ((err = PAPI_read(rt->EventSet, values)) != PAPI_OK) {
        report_error(__FILE__, __LINE__, "PAPI_read", err);
    }
    rt->cycles = values[1];
    if ((err = PAPI_stop(rt->EventSet, values)) != PAPI_OK) {
        report_error(__FILE__, __LINE__, "PAPI_stop", err);
    }
}



u_int64_t
rt_papi_nanos(rtimer_papi_t * rt)
{
    u_int64_t       nanos = 0;

    CHECK_VALID(rt);

    // printf(TWO12, values[0], values[1], "(INS/CYC)\n");
    nanos = (double) 1000.0 *rt->cycles / hwinfo->mhz;

    return nanos;
}
