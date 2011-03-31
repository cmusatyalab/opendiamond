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

/*
 * Common code to decode and parse scope cookies.
 */

#include <string.h>
#include <stdlib.h>
#include "scope_priv.h"

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

void string_hex_decode(GString *buf)
{
    unsigned int i;
    for (i = 0; i < buf->len; i += 2)
	buf->str[i/2] = (decode_nibble(buf->str[i]) << 4)|decode_nibble(buf->str[i+1]);
    buf->len /= 2;
}

static time_t scope_parse_expires(gchar *val)
{
    GTimeVal expires;
    if (!g_time_val_from_iso8601(val, &expires))
	return 0;
    return expires.tv_sec;
}

static gchar **scope_parse_servers(gchar *val)
{
    GPtrArray *a = g_ptr_array_new();
    gchar *tmp, **servers;
    unsigned int i;

    servers = g_strsplit_set(val, ";,", 0);

    /* strip whitespace and empty name entries */
    for (i = 0; servers[i]; i++)
    {
	tmp = g_strstrip(servers[i]);
	if (*tmp) g_ptr_array_add(a, g_strdup(tmp));
    }

    g_strfreev(servers);

    g_ptr_array_add(a, NULL);
    return (gchar **)g_ptr_array_free(a, FALSE);
}

struct scopecookie *scopecookie_parse(const gchar *cookie)
{
    struct scopecookie *scope;
    GString *content;
    gchar *eoh, *tmp, **lines;
    void *buf;
    gsize len;
    unsigned int i;

    if (!g_str_has_prefix(cookie, BEGIN_COOKIE))
	return NULL;

    content = g_string_new(cookie);
    g_string_erase(content, 0, strlen(BEGIN_COOKIE)+1);

    tmp = strstr(content->str, END_COOKIE);
    if (!tmp) {
	g_string_free(content, TRUE);
	return NULL;
    }
    g_string_truncate(content, tmp - content->str);

    buf = g_base64_decode(content->str, &len);

    g_string_free(content, TRUE);

    /* find end of cookie header */
    eoh = g_strstr_len(buf, len, "\n\n");
    if (!eoh) {
	g_free(buf);
	return NULL;
    }

    /* split header and into individual lines */
    tmp = g_strndup(buf, eoh - (gchar *)buf);
    lines = g_strsplit(tmp, "\n", 0);
    g_free(tmp);

    /* allocate a new scope cookie */
    scope = g_new0(struct scopecookie, 1);
    scope->rawdata = buf;
    scope->rawlen = len;
    scope->scopedata = eoh + 2;

    for (i = 0; lines[i]; i++)
    {
	tmp = strchr(lines[i], ':');
	if (!tmp) continue;
	tmp = g_strdup(g_strstrip(&tmp[1]));

	if (g_str_has_prefix(lines[i], "Version:"))
	    scope->version = atoi(tmp);

	else if (g_str_has_prefix(lines[i], "Serial:"))
	    uuid_parse(tmp, scope->serial);
							/* avoid memleak */
	else if (g_str_has_prefix(lines[i], "KeyId:") && !scope->keyid) {
	    scope->keyid = g_string_new(tmp);
	    string_hex_decode(scope->keyid);
	}
	else if (g_str_has_prefix(lines[i], "Expires:"))
	    scope->expires = scope_parse_expires(tmp);
							/* avoid memleak */
	else if (g_str_has_prefix(lines[i], "Servers:") && !scope->servers)
	    scope->servers = scope_parse_servers(tmp);

	g_free(tmp);
    }
    g_strfreev(lines);

    return scope;
}

void scopecookie_free(struct scopecookie *scope)
{
    if (!scope) return;
    g_strfreev(scope->servers);
    g_string_free(scope->keyid, TRUE);
    g_free(scope->rawdata);
    g_free(scope);
}

