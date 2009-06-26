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
 *
 */
#ifndef _G_CHECKSUM_COMPAT_H_
#define _G_CHECKSUM_COMPAT_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_GLIB2_OLD
#include <glib.h>
#define G_CHECKSUM_MD5 0x4d443500
GChecksum *g_checksum_new(int type);
void g_checksum_update(GChecksum *context, const guchar *input, gssize len);
void g_checksum_get_digest(GChecksum *context, guint8 *digest, gsize *len);
void g_checksum_free(GChecksum *context);
#endif

#endif /* _G_CHECKSUM_COMPAT_H_ */

