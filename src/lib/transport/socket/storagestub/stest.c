/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 2
 *
 *  Copyright (c) 2002-2005 Intel Corporation
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/time.h>
#include <netdb.h>
#include <string.h>
#include "diamond_consts.h"
#include "diamond_types.h"
#include "lib_tools.h"
#include "obj_attr.h"
#include "lib_odisk.h"
#include "socket_trans.h"
#include "lib_dctl.h"
#include "lib_sstub.h"


static char const cvsid[] =
    "$Header$";


#define	DATA_LEN	1024*1024*4
#define	ATTR_LEN	1024*1024*4

void           *lib_cookie;
void           *conn_cookie;


void
send_obj(void *ccookie, int vno)
{

	obj_data_t     *foo;
	char           *data;
	char           *attr;

	foo = (obj_data_t *) malloc(sizeof(*foo));
	if (foo == NULL) {
		perror("send_obj");
		exit(1);
	}

	data = (char *) malloc(DATA_LEN);
	memset(data, 0, DATA_LEN);
	if (data == NULL) {
		perror("allocate data:");
		exit(1);
	}
	attr = (char *) malloc(ATTR_LEN);
	memset(attr, 0, ATTR_LEN);
	if (attr == NULL) {
		perror("allocate attr:");
		exit(1);
	}


	foo->data_len = DATA_LEN;
	foo->data = data;
#ifdef	XXX

	foo->attr_info.attr_len = ATTR_LEN;
	foo->attr_info.attr_data = attr;
#endif

	sstub_send_obj(ccookie, foo, vno, 1);


}


/*
 * For this test we create a thread that just sends
 * and object, sleeps for a little and exits.
 */

void           *
main_thread(void *cookie)
{

	send_obj(cookie, 5);
	while (1) {
		sleep(5);
	}
	exit(0);
}


int
handle_new_connection(void *cookie, void **newcookie)
{
	pthread_t       thread;

	printf("stest:  new conn - cookie %p  !!! \n", cookie);
	conn_cookie = cookie;
	pthread_create(&thread, PATTR_DEFAULT, main_thread, cookie);
	/*
	 * XXX err code 
	 */
	*newcookie = (void *) 1;
	return (0);

}

int
close_connection(void *cookie)
{
	printf("stest: closeing conn \n");
	return (0);
}

int
start(void *cookie, int gen)
{
	printf("stest: start \n");
	return (0);
}


int
stop(void *cookie, int gen)
{
	printf("stest: stop \n");
	return (0);
}

int
set_searchlet(void *cookie, int gen, char *spec, char *filter)
{
	printf("stest: set searchlet %s %s  \n", spec, filter);
	return (0);
}


int
set_list(void *cookie, int gen)
{
	printf("stest: set list \n");
	return (0);
}

int
terminate(void *cookie, int gen)
{
	printf("stest: terminate \n");
	return (0);
}

void
get_stats(void *cookie, int gen)
{
	int             size;
	dev_stats_t    *dstats;
	printf("stest: get stats list \n");

	size = DEV_STATS_SIZE(1);

	dstats = (dev_stats_t *) malloc(sizeof(*dstats));
	if (dstats == NULL) {
		exit(1);
	}


	dstats->ds_objs_total = 400;
	dstats->ds_num_filters = 1;

	dstats->ds_filter_stats[0].fs_avg_exec_time = 0x123;
	dstats->ds_filter_stats[0].fs_objs_processed = 23;
	dstats->ds_filter_stats[0].fs_objs_dropped = 40;

	sstub_send_stats(conn_cookie, dstats, size);
}

int
release_obj(void *cookie, obj_data_t * obj)
{

	printf("release obj \n");
	free(obj->data);
	// XXX free(obj->attr_info.attr_data);
	free(obj);
	return (0);
}

int
dev_chars(void *cookie, int gen)
{

	device_char_t   dchar;

	printf("device_chars \n");
	sstub_send_dev_char(conn_cookie, &dchar);
	return (0);
}



int
main(int argc, char **argv)
{

	sstub_cb_args_t cb_args;

	/*
	 * Set the list of callback functions that we are going to use.
	 */
	cb_args.new_conn_cb = handle_new_connection;
	cb_args.close_conn_cb = close_connection;
	cb_args.start_cb = start;
	cb_args.stop_cb = stop;
	b_args.set_searchlet_cb = set_searchlet;
	cb_args.set_list_cb = set_list;
	cb_args.terminate_cb = terminate;
	cb_args.get_stats_cb = get_stats;
	cb_args.release_obj_cb = release_obj;
	cb_args.get_char_cb = dev_chars;



	lib_cookie = sstub_init(&cb_args);

	sstub_listen(lib_cookie, 1);

	exit(0);
}
