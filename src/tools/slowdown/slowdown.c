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
 * Program that slows the effective CPU speed by slowing down using
 * the fifo scheduling an sucking up cycles.
 */

#include <sys/io.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <math.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sched.h>
#include <errno.h>
#include <assert.h>

/* call to read the cycle counter */
#define rdtscll(val) __asm__ __volatile__("rdtsc" : "=A" (val))


unsigned long long
read_cycle()
{
	unsigned long long	foo;

	rdtscll(foo);

	return(foo);

}

void
usage()
{
	fprintf(stdout, "slowdown -r <ratio> [-v][-p <pid>\n");
	fprintf(stdout, "\t-r <ratio> : consume ratio percent of cpu \n");
	fprintf(stdout, "\t-v         : verbose mode for debugging \n");
	fprintf(stdout, "\t-p <pid>   : exit when pid is no longer running \n");
}


#define	MAX_NAME		128

int
main(int argc, char* argv[])
{
	struct sched_param 	params;
	double			target = 0.0;
	unsigned long long	wall_start;
	unsigned long long	wall_elapsed;
	unsigned long long	wait_cum = 0;
	unsigned long long	wait_start;
	unsigned long long	wait_end;
	unsigned long long	temp_cycle;
	unsigned long long	delta;
	double			ratio;
	int			err;
	int			have_pid = 0;
	extern char *		optarg;
	int			target_set = 0;
	int			verbose = 0;
	char			proc_dir[MAX_NAME];
	struct	stat		stat_buf;
	int			c;

	/*
	 * The command line options.
	 */
	while (1) {
		c = getopt(argc, argv, "hp:r:v");
		if (c == -1) {
			break;
		}

		switch (c) {
			case 'h':
				usage();
				exit(0);
				break;

			case 'r':
				target = atof(optarg)/100.0;
				target_set = 1;
				break;

			case 'p':
				err = snprintf(proc_dir, MAX_NAME, "%s/%s", "/proc", optarg);
				if (err >= MAX_NAME) {
					fprintf(stderr, "path name too long: increase MAX_NAME\n");
					exit(1);
				}
				have_pid = 1;
				break;

			case 'v':
				verbose = 1;
				break;

			default:
				printf("unknown option %c\n", c);
				break;
		}
	}

	if (target_set == 0) {
		usage();
		exit(1);
	}

	if (verbose) {
		fprintf(stdout, "target ration: %f \n", target);
		if (have_pid) {
			fprintf(stdout, "parent_proc = %s \n", proc_dir);
		}
	}

	/*
	 * Setup the scheduler so we do real-time scheduling.
	 */
	sched_getparam(0, &params);
	params.sched_priority = 50;
	if (sched_setscheduler(0, SCHED_FIFO, &params)) {
		perror("Failed to set scheduler");
		exit(0);
	}

	/*
	 * Get our initial time.
	 */
	wall_start = read_cycle();

	while (1) {
		wait_start = read_cycle();

		if (have_pid) {
			err = stat(proc_dir, &stat_buf);
			if (err == -1) {
				printf("Stat failed\n");
				exit(0);
			}
		}


		wall_elapsed = wait_start - wall_start;
		delta = (unsigned long long) (((double)wall_elapsed) *
		                              target) - wait_cum;

		if (verbose) {
			fprintf(stdout, "new delta: %lld (wall %lld wait %lld)\n",
			        delta, wall_elapsed, wait_cum);
			ratio = ((double) wait_cum) / ((double)wall_elapsed);
			fprintf(stdout, "ratio %16.12f \n", ratio);
			ratio = (((double) wait_cum) + ((double)delta))/ ((double)wall_elapsed);
			fprintf(stdout, "ratio %16.12f \n", ratio);
		}

		wait_end = wait_start + delta;

		temp_cycle = read_cycle();
		while (temp_cycle < wait_end) {
			temp_cycle = read_cycle();
		}

		wait_cum += temp_cycle - wait_start;
		usleep(10000);

	}
	return 0;
}

