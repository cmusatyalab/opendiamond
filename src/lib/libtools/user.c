/*
 *      OpenDiamond 2.0
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


#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include "lib_tools.h"


static char const cvsid[] =
    "$Header$";

/*
 * XXXX move this else where 
 */
void
get_user_name(char *name)
{
	uid_t           uid;
	struct passwd  *pwd;
	size_t          ret;

	uid = getuid();

	pwd = getpwuid(uid);

	/*
	 * if we fail, the name is the uid otherwise use the name 
	 */
	if (pwd == NULL) {
		ret = snprintf(name, MAX_USER_NAME, "%d", uid);
	} else {
		ret = snprintf(name, MAX_USER_NAME, "%s", pwd->pw_name);
	}

}
