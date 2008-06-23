/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 3
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

#ifndef _LIB_TOOLS_H_
#define _LIB_TOOLS_H_

/* pull in all the headers that make up the library */
#include "sig_calc.h"


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
diamond_public
void get_user_name(char *name);

#ifdef __cplusplus
}
#endif


#endif                          /* _LIB_TOOOLS_H */
