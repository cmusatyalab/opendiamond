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

#ifndef _URI_UTIL_H_
#define _URI_UTIL_H_

#include <glib.h>

/* resolve relative URL wrt a base URL according to RFC 1808 section 4 */
gchar *uri_normalize(const gchar *url, const gchar *base);

#endif /* _URI_UTIL_H_ */

