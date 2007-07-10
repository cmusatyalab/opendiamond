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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>

#include <glib.h>

#include "lib_log.h"


void
string_test()
{
	int             i;

	/*
	 * Set the levels to where we want them to be.
	 */
	log_settype(LOGT_APP);
	log_setlevel(LOGL_INFO);

	/*
	 * Write a bunch of strings of different lengths and contents
	 */
	for (i = 0; i < 1; i++) {
		log_message(LOGT_APP, LOGL_INFO, "A\n");
		log_message(LOGT_APP, LOGL_INFO, "BB");
		log_message(LOGT_APP, LOGL_INFO, "CCC");
		log_message(LOGT_APP, LOGL_INFO, "DDD");
		log_message(LOGT_APP, LOGL_INFO, "EEEE");
		log_message(LOGT_APP, LOGL_INFO, "%s %s ", "sldkfj", "abc");
	}
}


int
main(int argc, char **argv)
{
	void        *foo;
	pthread_t	log_thread;
	int 		err;

	log_init("testlog", NULL, &foo);
	string_test();
	sleep(5);
	log_term(foo);
	return (0);
}
