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
#include "lib_od.h"
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

		} else {
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
	char path_name[NAME_MAX];
	int	err;
	uint64_t	oid;
	pr_obj_t *	pr_obj;
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

	get_oid_loop(odisk);
	exit(0);
}
