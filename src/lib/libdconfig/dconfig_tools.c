/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 2
 *
 *  Copyright (c) 2002-2005 Intel Corporation
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#include    <sys/types.h>
#include    <unistd.h>
#include    <stdio.h>
#include    <stdlib.h>
#include    <stdint.h>
#include    <assert.h>
#include    "diamond_consts.h"
#include    "diamond_types.h"
#include    "lib_dconfig.h"


static char const cvsid[] =
    "$Header$";



#define	MAX_PATH				256


/*
 * This looks for the config file in a couple of places.
 * the first place is in $DIAMOND_CONFIG.  Next it looks in
 * the local directory, and third it looks in the system defined
 * location.
 */

FILE           *
dconfig_open_config_file(const char *conf_file)
{
	FILE           *new_file;
	char           *lkup;
	char            fname[MAX_PATH];


	/*
	 * try the environment variable 
	 */
	lkup = getenv(DIAMOND_CONFIG_ENV_NAME);
	if (lkup != NULL) {
		/*
		 * XXX deal with overflow 
		 */
		snprintf(fname, MAX_PATH, "%s/%s", lkup, conf_file);
		fname[MAX_PATH - 1] = '\0';
		new_file = fopen(fname, "r");
		if (new_file != NULL) {
			return (new_file);
		}
	}


	/*
	 * look in the local directory for the config directory 
	 */
	snprintf(fname, MAX_PATH, "./%s/%s", DIAMOND_CONFIG_DIR_NAME,
		 conf_file);
	fname[MAX_PATH - 1] = '\0';
	new_file = fopen(fname, "r");
	if (new_file != NULL) {
		return (new_file);
	}

	/*
	 * try the user's home directory 
	 */
	lkup = getenv("HOME");
	if (lkup != NULL) {
		/*
		 * XXX deal with overflow 
		 */
		snprintf(fname, MAX_PATH, "%s/%s/%s", lkup,
			 DIAMOND_CONFIG_DIR_NAME, conf_file);
		fname[MAX_PATH - 1] = '\0';
		new_file = fopen(fname, "r");
		if (new_file != NULL) {
			return (new_file);
		}
	}
	/*
	 * didn't find a file, return NULL 
	 */
	return (NULL);
}
