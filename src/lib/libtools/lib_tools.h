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

/*
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */


#ifndef _LIB_TOOLS_H_
#define _LIB_TOOLS_H_

/* pull in all the headers that make up the library */
#include "rstat.h"
#include "ring.h"
#include "queue.h"
#include "sig_calc.h"
#include "rtimer.h"
#include "rcomb.h"
#include "rgraph.h"
#include "rstat.h"


/* external functions defined in user.c */

/* max user name we support */
#define MAX_USER_NAME   64

#ifdef __cplusplus
extern          "C"
{
#endif

/*
 * fill in string name of the current user (assumes
 * string is MAX_USER_NAME bytes long).
 */
void get_user_name(char *name);

/* test if a given file exists return is 1 if yes, 0 if not */
int file_exists(const char *name);
int file_get_lock(const char *fname);
int file_release_lock(const char *fname);


#ifdef __cplusplus
}
#endif


#endif                          /* _LIB_TOOOLS_H */
