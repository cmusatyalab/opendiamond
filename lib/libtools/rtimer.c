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

#include <stdio.h>
#include <stdlib.h>
#include "rtimer.h"


void
rt_start(rtimer_t * rt)
{
  if (clock_gettime(CLOCK_MONOTONIC, &rt->tp1) != 0) {
    perror("clock_gettime");
    exit(1);
  }
}


void
rt_stop(rtimer_t * rt)
{
  if (clock_gettime(CLOCK_MONOTONIC, &rt->tp2) != 0) {
    perror("clock_gettime");
    exit(1);
  }
}


rtime_t
rt_nanos(rtimer_t * rt)
{
  u_int64_t       nanos = 0;
  static const u_int64_t ns_s = 1000000000;
  nanos +=
    (u_int64_t) (rt->tp2.tv_sec -
		 rt->tp1.tv_sec) * ns_s;
  nanos +=
    (u_int64_t) (rt->tp2.tv_nsec -
		 rt->tp1.tv_nsec);

  return nanos;
}


double
rt_time2secs(rtime_t t)
{
  double          secs = t;
  secs /= 1000000000.0;
  return secs;
}
