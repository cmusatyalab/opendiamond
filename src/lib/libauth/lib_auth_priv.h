/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 3
 *
 *  Copyright (c) 2002-2007 Intel Corporation
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#ifndef LIB_AUTH_PRIV_H_
#define LIB_AUTH_PRIV_H_

#include <krb5.h>
#include <config.h>

#ifdef HAVE_HEIMDAL
#define tkt_client(t)   ((t)->client)
#define err_text(e)		((e)->e_text)
#define err_length(e)	(strlen((e)->e_text))
#else
#define tkt_client(t)   ((t)->enc_part2->client)
#define err_text(e)		((e)->text.data)
#define err_length(e)	((e)->text.length)
#endif

/*
 * private data for authentication 
 */
typedef struct auth_context {
	krb5_context context;
	krb5_auth_context auth_context;
} auth_context_t;


#endif /*LIB_AUTH_PRIV_H_*/
