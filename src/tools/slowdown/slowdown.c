/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 3
 *
 *  Copyright (c) 2002-2005 Intel Corporation
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

/*
 * Program that slows the effective CPU speed by slowing down using
 * the fifo scheduling an sucking up cycles.
 */

#include <sys/io.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <sched.h>
#include <errno.h>
#include <assert.h>


static char const cvsid[] = "$Header$";

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

