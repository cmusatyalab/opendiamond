/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 4
 *
 *  Copyright (c) 2009 Carnegie Mellon University
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#ifndef _SCOPE_PRIV_H_
#define _SCOPE_PRIV_H_

#include <uuid/uuid.h>
#include <glib.h>

/* a scope cookie consists of a base64 encoded data delimited by the following
 * BEGIN_COOKIE and END_COOKIE markers. */
#define BEGIN_COOKIE "-----BEGIN OPENDIAMOND SCOPECOOKIE-----"
#define END_COOKIE   "-----END OPENDIAMOND SCOPECOOKIE-----"

/* The base64 encoded data consists of the following,
 *
 *	<hexadecimally encoded signature of all following data>\n
 *	Version: 1\n
 *	Serial: <uuid>\n
 *	KeyId: <string identifying the signing certificate>\n
 *	Expires: <ISO-8601 timestamp>\n
 *	Servers: <server1>;<server2>;<server3>
 *	\n
 *	<scope body>
 */

#define SCOPECOOKIE_VERSION 1

struct scopecookie {
    unsigned int version; /* version */
    uuid_t serial;	/* unique identifier */
    GString *keyid;	/* identifies certificate that signed the cookie */
    time_t expires;	/* Unix time when cookie expires */
    gchar **servers;	/* list of servers that will accept the cookie */
    gchar *scopedata;	/* points into rawdata, indicating scope body location */
    gchar *rawdata;
    gsize rawlen;
};

struct scopecookie *scopecookie_parse(const gchar *cookie);
void scopecookie_free(struct scopecookie *cookie);

/* only used by adiskd */
int scopecookie_validate(struct scopecookie *cookie);

void string_hex_encode(GString *buf);
void string_hex_decode(GString *buf);

#endif /* _SCOPE_PRIV_H_ */
