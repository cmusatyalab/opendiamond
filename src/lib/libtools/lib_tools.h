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


#ifndef _LIB_TOOLS_H_
#define _LIB_TOOLS_H_

#include "rstat.h"



/* external functions defined in user.c */

/* max user name we support */
#define MAX_USER_NAME   64

/* 
 * fill in string name of the current user (assumes
 * string is MAX_USER_NAME bytes long).
 */
void    get_user_name(char *name);


#endif                          /* _LIB_TOOOLS_H */
