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
obj_fetch(void *arg)
{
	odisk_state_t *	odisk = (odisk_state_t *)arg;
	obj_data_t *	new_obj;
	int		err;
	int		cnt = 0;
	struct timeval	tv;
	struct timezone tz;
	double		start, endt, difft;
	double		rate;

	gettimeofday(&tv, &tz);
	start = (double)tv.tv_sec + (double)tv.tv_sec/(double)1000000.0;
	while (1) {
		err = odisk_next_obj(&new_obj, odisk);
		if (err == ENOENT) {
			gettimeofday(&tv, &tz);
			endt = (double)tv.tv_sec + (double)tv.tv_sec/(double)1000000.0;
			difft = endt - start;
			rate = (double)cnt/difft;

			printf("search done: time %f rate %f fetch %d cnt %d\n",
			       difft, rate, pr_fetch, cnt);
			exit(0);
		} else if (err) {
		}
		else {
			cnt++;
			odisk_release_obj(new_obj);
		}
	}

}

static void
mark_end()
{
	pr_obj_t * pr_obj;

	pr_obj = (pr_obj_t *) malloc( sizeof(*pr_obj) );
	pr_obj->obj_id = 0;
	pr_obj->filters = NULL;
	pr_obj->fsig = NULL;
	pr_obj->iattrsig = NULL;
	//pr_obj->oattr_fname = NULL;
	pr_obj->oattr_fnum = -1;
	pr_obj->stack_ns = 0;
	odisk_pr_add(pr_obj);
}

static void *
get_oid_loop(void *arg)
{
	odisk_state_t *	odisk = (odisk_state_t *)arg;
	int	err;
	uint64_t	oid;
	pr_obj_t *	pr_obj;

	while (1) {
		err = odisk_read_next_oid(&oid, odisk);
		if (err == 0) {
			pr_obj = (pr_obj_t *) malloc(sizeof(*pr_obj));
			pr_obj->obj_id = oid;
			pr_obj->filters = NULL;
			pr_obj->fsig = NULL;
			pr_obj->iattrsig = NULL;
			pr_obj->oattr_fnum = 0;
			pr_obj->stack_ns = 0;
			odisk_pr_add(pr_obj);
			pr_fetch++;
		} else if (err == ENOENT) {
			printf("get oid done \n");
			mark_end();
			break;
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
	pthread_t 	id;

	log_init(&log_cookie);
	dctl_init(&dctl_cookie);

	err = odisk_init(&odisk, "/opt/dir1", dctl_cookie, log_cookie);
	if (err) {
		errno = err;
		perror("failed to init odisk");
		exit(1);
	}

	odisk_set_gid(odisk, gid);
	odisk_reset(odisk);

	err = pthread_create(&id, PATTR_DEFAULT, obj_fetch, (void *)odisk);
	err = pthread_create(&id, PATTR_DEFAULT, get_oid_loop, (void *)odisk);

	while (1) {
		sleep(10);
	}
	exit(0);
}
