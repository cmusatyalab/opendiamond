/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 5
 *
 *  Copyright (c) 2002-2005 Intel Corporation
 *  Copyright (c) 2010 Carnegie Mellon University
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#ifndef _RTIMER_H_
#define _RTIMER_H_

#include <sys/types.h>
#include <time.h>
#include <sys/resource.h>
#include <unistd.h>

struct rtimer_t
{
  struct timespec tp1;
  struct timespec tp2;
};

#ifdef __cplusplus
extern          "C"
{
#endif

  struct rtimer_t;
  typedef struct rtimer_t rtimer_t;

  typedef u_int64_t rtime_t;

  void     rt_start(rtimer_t * rt);
  void     rt_stop(rtimer_t * rt);
  rtime_t  rt_nanos(rtimer_t * rt);

  double   rt_time2secs(rtime_t t);

#ifdef __cplusplus
}
#endif

#endif                          /* _RTIMER_H_ */
