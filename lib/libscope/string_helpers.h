/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 4
 *
 *  Copyright (c) 2011 Carnegie Mellon University
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#ifndef _STRING_HELPERS_H_
#define _STRING_HELPERS_H_

#include <glib.h>

void string_hex_encode(GString *buf);
void string_hex_decode(GString *buf);

void string_base64_encode(GString *buf);

#endif /* _STRING_HELPERS_H_ */
