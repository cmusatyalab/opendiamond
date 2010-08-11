/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 4
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

#ifndef _TOOLS_PRIV_H_
#define _TOOLS_PRIV_H_

#ifdef __cplusplus
extern          "C"
{
#endif


int file_get_lock(const char *fname);
int file_release_lock(const char *fname);




#ifdef __cplusplus
}
#endif


#endif
