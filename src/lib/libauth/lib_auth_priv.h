/*
 * 	Diamond (Release 1.0)
 *      A system for interactive brute-force search
 *
 *      Copyright (c) 2002-2006, Intel Corporation
 *      All Rights Reserved
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#ifndef LIB_AUTH_PRIV_H_
#define LIB_AUTH_PRIV_H_

#include <krb5.h>

/*
 * private data for authentication 
 */
typedef struct auth_context {
	krb5_context context;
	krb5_auth_context auth_context;
} auth_context_t;

#endif /*LIB_AUTH_PRIV_H_*/
