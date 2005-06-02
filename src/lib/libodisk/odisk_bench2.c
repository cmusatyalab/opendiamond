/*
 * 	Diamond (Release 1.0)
 *      A system for interactive brute-force search
 *
 *      Copyright (c) 2002-2005, Intel Corporation
 *      All Rights Reserved
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <stdint.h>
#include <unistd.h>
#include "diamond_consts.h"
#include "diamond_types.h"
#include "rtimer.h"
#include "lib_odisk.h"
#include "lib_dctl.h"
#include "lib_log.h"
#include "odisk_priv.h"

int	pr_fetch = 0;



static void *
get_oid_loop(void *arg)
{
	odisk_state_t *	odisk = (odisk_state_t *)arg;
	char path_name[NAME_MAX];
	int	err;
	uint64_t	oid;
	int		cnt = 0;
	struct timeval	tv;
	struct timezone tz;
	double		start, endt, difft;
	double		rate;
	obj_data_t *	new_obj;

	gettimeofday(&tv, &tz);
	start = (double)tv.tv_sec + (double)tv.tv_sec/(double)1000000.0;
	while (1) {
		err = odisk_read_next_oid(&oid, odisk);
		if (err == 0) {
			sprintf(path_name, "%s/OBJ%016llX",
			        odisk->odisk_path, oid);
			err = odisk_load_obj(odisk, &new_obj, path_name);
			if (err) {
				printf("load obj <%s> failed %d \n",
				       path_name, err);
			} else {
				cnt++;
				odisk_release_obj(new_obj);
			}
		} else if (err == ENOENT) {
			gettimeofday(&tv, &tz);
			endt = (double)tv.tv_sec + (double)tv.tv_sec/(double)1000000.0;
			difft = endt - start;
			rate = (double)cnt/difft;

			printf("search done: time %f rate %f cnt %d\n",
			       difft, rate, cnt);
			exit(0);
		} else {
			printf("unknown error \n");
		}
	}
	return(NULL);
}


int
main(int argc, char **argv)
{
	odisk_state_t*	odisk;
	void *		log_cookie;
	void *		dctl_cookie;
	gid_t 	gid = 0x040001;
	int		err;

	log_init(&log_cookie);
	dctl_init(&dctl_cookie);

	err = odisk_init(&odisk, NULL, dctl_cookie, log_cookie);
	if (err) {
		errno = err;
		perror("failed to init odisk");
		exit(1);
	}

	odisk_set_gid(odisk, gid);
	odisk_reset(odisk);

	get_oid_loop(odisk);
	exit(0);
}
