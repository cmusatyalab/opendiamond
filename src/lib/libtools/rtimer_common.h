
#ifdef __cplusplus
extern          "C" {
#endif

    struct rtimer_t;
    typedef struct rtimer_t rtimer_t;

    typedef u_int64_t rtime_t;

    extern void     rt_init(rtimer_t * rt);
    extern void     rt_start(rtimer_t * rt);
    extern void     rt_stop(rtimer_t * rt);
    extern rtime_t  rt_nanos(rtimer_t * rt);

    extern double   rt_time2secs(rtime_t t);



#ifdef __cplusplus
}
#endif
