/*
 * 	SnapFind (Release 0.9)
 *      An interactive image search application
 *
 *      Copyright (c) 2002-2005, Intel Corporation
 *      All Rights Reserved
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <errno.h>
#include <sys/time.h>
#include <math.h>
#include <libgen.h>
#include <stdint.h>
#include <limits.h>
#include <ctype.h>
#include <assert.h>

#include "lib_searchlet.h"
#include "lib_filter.h"
#include "ring.h"
#include "face.h"               /* XXX hack */


#define	MAX_PATH	128
#define	MAX_NAME	128

/*
 * this needs to be synced with the snapfind searchlets 
 */
#define	DISPLAY_NAME "Display-Name"
/*
 * this need to be global to make our life easier
 */
ls_search_handle_t shandle;


/*!
 * Convert a colon seperated string into a 64-bit value.
 */
uint64_t
parse_uint64_string(const char *s)
{
	int             i,
	o;
	unsigned int    x;          // Will actually hold an unsigned char
	uint64_t        u = 0u;

	assert(s);
	for (i = 0; i < 8; i++) {
		o = 3 * i;
		assert(isxdigit(s[o]) && isxdigit(s[o + 1]));
		assert((s[o + 2] == ':') || (s[o + 2] == '\0'));
		sscanf(s + o, "%2x", &x);
		u <<= 8;
		u += x;
	}
	return u;
}


/*!
 * This initializes the search  state.
 */

void
init_search()
{

	/*!
 	 * Call search init and get a handle.
 	 */
	shandle = ls_init_search();
	if (shandle == NULL) {
		printf("failed to initialize:  !! \n");
		exit(1);
	}

}

void
set_searchlist(int n, groupid_t * gids)
{
	int             err;

	err = ls_set_searchlist(shandle, n, gids);
	if (err) {
		printf("Failed to set searchlist on  err %d \n", err);
		exit(1);
	}
}

int
load_searchlet(char *obj_file, char *spec_file)
{
	int             err;
	char           *full_spec;
	char           *full_obj;
	char           *path_name;
	char           *res;

	full_spec = (char *) malloc(MAX_PATH);
	if (full_spec == NULL) {
		exit(1);                /* XXX */
	}
	full_obj = (char *) malloc(MAX_PATH);
	if (full_obj == NULL) {
		exit(1);                /* XXX */
	}

	path_name = (char *) malloc(MAX_PATH);
	if (path_name == NULL) {
		exit(1);                /* XXX */
	}

	res = getcwd(path_name, MAX_PATH);
	if (res == NULL) {
		exit(1);
	}

	sprintf(full_obj, "%s/%s", path_name, obj_file);
	sprintf(full_spec, "%s/%s", path_name, spec_file);

	if (obj_file[0] == '/') {
		err = ls_set_searchlet(shandle, DEV_ISA_IA32, obj_file, full_spec);
	} else {
		err = ls_set_searchlet(shandle, DEV_ISA_IA32, full_obj, full_spec);
	}
	if (err) {
		printf("Failed to set searchlet on err %d \n", err);
		exit(1);
	}

	return (0);
}


#define	MAX_DEVS	24
#define	MAX_FILTS	24

void
dump_dev_stats(dev_stats_t * dev_stats, int stat_len)
{
	int             i;

	fprintf(stdout, "Total objects: %d \n", dev_stats->ds_objs_total);
	fprintf(stdout, "Processed objects: %d \n", dev_stats->ds_objs_processed);
	fprintf(stdout, "Dropped objects: %d \n", dev_stats->ds_objs_dropped);
	fprintf(stdout, "System Load: %d \n", dev_stats->ds_system_load);
	fprintf(stdout, "Avg obj time: %lld \n", dev_stats->ds_avg_obj_time);

	for (i = 0; i < dev_stats->ds_num_filters; i++) {
		fprintf(stdout, "\tFilter: %s \n",
		        dev_stats->ds_filter_stats[i].fs_name);
		fprintf(stdout, "\tProcessed: %d \n",
		        dev_stats->ds_filter_stats[i].fs_objs_processed);
		fprintf(stdout, "\tDropped: %d \n",
		        dev_stats->ds_filter_stats[i].fs_objs_dropped);
		/* JIAYING */
		fprintf(stdout, "\tCACHE Dropped: %d \n",
		        dev_stats->ds_filter_stats[i].fs_objs_cache_dropped);
		fprintf(stdout, "\tCACHE Passed: %d \n",
		        dev_stats->ds_filter_stats[i].fs_objs_cache_passed);
		fprintf(stdout, "\tCACHE Compute: %d \n",
		        dev_stats->ds_filter_stats[i].fs_objs_compute);
		/* JIAYING */
		fprintf(stdout, "\tAvg Time: %lld \n\n",
		        dev_stats->ds_filter_stats[i].fs_avg_exec_time);

	}
}


void
dump_stats()
{
	int             err,
	i;
	int             dev_count;
	ls_dev_handle_t dev_list[MAX_DEVS];
	dev_stats_t    *dev_stats;
	int             stat_len;


	dev_count = MAX_DEVS;
	err = ls_get_dev_list(shandle, dev_list, &dev_count);
	if (err) {
		fprintf(stderr, "Failed to get device list %d \n", err);
		exit(1);
	}

	dev_stats = (dev_stats_t *) malloc(DEV_STATS_SIZE(MAX_FILTS));
	assert(dev_stats != NULL);

	for (i = 0; i < dev_count; i++) {
		stat_len = DEV_STATS_SIZE(MAX_FILTS);
		err = ls_get_dev_stats(shandle, dev_list[i], dev_stats, &stat_len);
		if (err) {
			fprintf(stderr, "Failed to get device list %d \n", err);
			exit(1);
		}
		dump_dev_stats(dev_stats, stat_len);
	}

	free(dev_stats);

	fflush(stdout);
}

void
dump_id(ls_obj_handle_t handle)
{
	off_t           size;
	int             err;
	obj_id_t        obj_id;

	size = sizeof(obj_id);
	err = lf_read_attr(handle, "MY_OID", &size, (char *) &obj_id);
	if (err) {
		fprintf(stdout, "OID Unknown ");
	} else {
		fprintf(stdout, "OID %016llX.%016llX ",
		        obj_id.dev_id, obj_id.local_id);

	}
}

void
dump_name(ls_obj_handle_t handle)
{
	off_t           size;
	int             err;
	int             i;
	char            big_buffer[MAX_NAME];

	size = MAX_NAME;
	err = lf_read_attr(handle, DISPLAY_NAME, &size, (char *) big_buffer);
	if (err) {
		fprintf(stdout, "name: Unknown ");
	} else {
		for (i = 0; i < strlen(big_buffer); i++) {
			if (big_buffer[i] == ' ') {
				big_buffer[i] = '_';
			}
		}
		fprintf(stdout, "name: %s ", big_buffer);
	}
}

void
dump_matches(ls_obj_handle_t handle)
{
	off_t           size;
	int             err;
	char            big_buffer[MAX_NAME];
	int             num_histo;
	search_param_t  param;
	int             i;
	double          prod = 1.0;
	double          sum = 0.0;

	size = sizeof(num_histo);
	err = lf_read_attr(handle, NUM_HISTO, &size,
	                   (char *) &num_histo);
	if (err) {
		return;
	}

	for (i = 0; i < num_histo; i++) {

		sprintf(big_buffer, HISTO_BBOX_FMT, i);
		size = sizeof(param);
		err = lf_read_attr(handle, big_buffer, &size,
		                   (char *) &param);

		if (!err) {
			prod *= (1.0 - param.distance);
			sum += (1.0 - param.distance);
			// fprintf(stdout, "%s %f ", param.name, (1.0 - param.distance));
		}
	}
	fprintf(stdout, "prod %f sum %f ", prod, sum);
}

/*
 * This function initiates the search by building a filter
 * specification, setting the searchlet and then starting the search.
 */
int
do_bench_search()
{
	int             err;
	int             count = 0;
	ls_obj_handle_t cur_obj;

	/*
	 * Go ahead and start the search.
	 */
	err = ls_start_search(shandle);
	if (err) {
		printf("Failed to start search on err %d \n", err);
		exit(1);
	}

	fprintf(stderr, "starting loop \n");
	fflush(stderr);
	while (1) {
		err = ls_next_object(shandle, &cur_obj, 0);
		if (err == ENOENT) {
			return (count);
		} else if (err == 0) {
			count++;
			dump_id(cur_obj);
			dump_name(cur_obj);
			dump_matches(cur_obj);
			fprintf(stdout, "\n");
			fflush(stdout);
			ls_release_object(shandle, cur_obj);
		} else {
			fprintf(stderr, "get_next_obj: failed on %d \n", err);
			exit(1);
		}
	}
	fprintf(stderr, "ending loop \n");
	fflush(stderr);

}

int
configure_devices(char *config_file)
{
	int             err;

	sleep(5);
	err = system(config_file);

	return (err);

}

void
usage()
{
	fprintf(stdout, "snap_bench -c <config_file> -f <filter_spec> ");
	fprintf(stdout, "-s <searchlet> -g <gid> \n");
}


#define	MAX_GROUPS	64

int
main(int argc, char **argv)
{
	int             err;
	char           *searchlet_file = NULL;
	char           *fspec_file = NULL;
	char           *config_file = NULL;
	int             gid_count = 0;
	groupid_t       gid_list[MAX_GROUPS];
	struct timeval  tv1,
				tv2;
	struct timezone tz;
	int             secs;
	int             usec;
	int             c,
	count;


	/*
	 * Parse the command line args.
	 */
	while (1) {
		c = getopt(argc, argv, "c:f:g:hs:");
		if (c == -1) {
			break;
		}

		switch (c) {

			case 'c':
				config_file = optarg;
				break;

			case 'f':
				fspec_file = optarg;
				break;

			case 'g':
				gid_list[gid_count] = parse_uint64_string(optarg);
				gid_count++;
				assert(gid_count < MAX_GROUPS);
				break;

			case 'h':
				usage();
				exit(0);
				break;

			case 's':
				searchlet_file = optarg;
				break;

			default:
				fprintf(stderr, "unknown option %c\n", c);
				usage();
				exit(1);
				break;
		}
	}


	if ((searchlet_file == NULL) || (fspec_file == NULL) ||
	    (config_file == NULL) || (gid_count == 0)) {

		usage();
		exit(1);
	}



	/*
	 * setup the search 
	 */
	init_search();

	set_searchlist(gid_count, gid_list);

	err = load_searchlet(searchlet_file, fspec_file);

	err = configure_devices(config_file);

	/*
	 * get start time 
	 */
	err = gettimeofday(&tv1, &tz);

	count = do_bench_search();

	/*
	 * get stop time 
	 */
	err = gettimeofday(&tv2, &tz);

	secs = tv2.tv_sec - tv1.tv_sec;
	usec = tv2.tv_usec - tv1.tv_usec;
	if (usec < 0) {
		usec += 1000000;
		secs -= 1;
	}

	fprintf(stdout, " Found %d items in %d.%d  \n", count, secs, usec);
	fflush(stdout);
	sleep(1);
	dump_stats();

	exit(0);
}
