/*
 *
 *
 *                          Diamond 1.0
 * 
 *            Copyright (c) 2002-2004, Intel Corporation
 *                         All Rights Reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 *    * Neither the name of Intel nor the names of its contributors may
 *      be used to endorse or promote products derived from this software 
 *      without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


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
