/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 4
 *
 *  Copyright (c) 2009-2011 Carnegie Mellon University
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

/*
 * Common code to encode and decode strings.
 */

#include <string.h>
#include "string_helpers.h"

/* encode string to hexadecimal representation */
void string_hex_encode(GString *buf)
{
    const gchar *hx = "0123456789abcdef";
    gsize old_len = buf->len;
    guchar *c, *p;
    unsigned int i;

    g_string_set_size(buf, old_len * 2);

    c = (guchar *)&buf->str[old_len-1];
    p = (guchar *)&buf->str[buf->len-1];

    for (i = old_len; i > 0; i--, c--) {
	*(p--) = hx[*c & 0xf];
	*(p--) = hx[*c >> 4];
    }
}

static int decode_nibble(unsigned char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

/* decode hexadecimal string */
void string_hex_decode(GString *buf)
{
    unsigned int i;

    for (i = 0; i < buf->len; i += 2)
	buf->str[i/2] = (decode_nibble(buf->str[i]) << 4) |
			 decode_nibble(buf->str[i+1]);

    g_string_set_size(buf, buf->len / 2);
}


/* base64 encode string and wrap lines at 64-characters */
void string_base64_encode(GString *buf)
{
    gchar *tmp;
    gsize i, len, n = 64;

    tmp = g_base64_encode((void *)buf->str, buf->len);
    len = strlen(tmp);

    g_string_assign(buf, "");
    for (i = 0; len; i += n, len -= n)
    {
	if (len < n) n = len;
	g_string_append_len(buf, &tmp[i], n);
	g_string_append_c(buf, '\n');
    }
    g_free(tmp);
}

