/*
 *      Diamond (Release 1.0)
 *      A system for interactive brute-force search
 *
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */


#include <pthread.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <limits.h>
#include "lib_tools.h"

static char const cvsid[] =
    "$Header$";

/*  max time to wait for a file lock before deciding to break it */
#define	MAX_LOCK_WAIT	5


/*
 * Test for the existence of a particular file.
 */
int
file_exists(const char *fname)
{

	struct stat stats;
	int err;
	
	err = stat(fname, &stats);
	if (err == 0)
		return(1);

	return(0);
}

int
file_get_lock(const char *fname)
{

	char namebuf[PATH_MAX];
	struct timespec  sleeptime;
	struct timeval	start, cur;
	int	fd;

	snprintf(namebuf, PATH_MAX, "%s.diamond_lock", fname);

	gettimeofday(&start, NULL);

	while ((fd = open(namebuf, O_CREAT|O_EXCL, 0400)) < 0) {
		if (errno != EEXIST) {
			fprintf(stderr, "Unable to get lock (%s) \n", namebuf);
			perror("mkdir failed:");
			assert(0);
		}
		/* if we have waited too long this may be an orphan */
		gettimeofday(&cur, NULL);
		if ((cur.tv_sec - start.tv_sec) > MAX_LOCK_WAIT) {
			fprintf(stderr, "Busting lock on (%s) \n", namebuf);
			if (unlink(namebuf) < 0)
				perror("Failed to bust lock:");
			start = cur;
		}

		/* sleep a short time and retry	 */
		sleeptime.tv_sec = 0;
		sleeptime.tv_nsec = 10000000;	/* 10 ms */
		nanosleep(&sleeptime, NULL);
	}
	close(fd);
	return(0);
}

int
file_release_lock(const char *fname)
{

	char namebuf[PATH_MAX];

	snprintf(namebuf, PATH_MAX, "%s.diamond_lock", fname);

	if (unlink(namebuf) < 0) {
		if (errno == ENOENT) {
			fprintf(stderr, "release lock: already released \n");
		} else {
			perror("Failed to release lock:");

		}
	}
	return(0);
}
