/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 4
 *
 *  Copyright (c) 2007-2009 Carnegie Mellon University
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

/*
 * This library provides the main functions of the dynamic
 * metadata scoping API.
 */

#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>
#include "lib_scope.h"
#include "scope_priv.h"


#define MAX_ROTATE_FILENO 9

/* rotate a set of files */
static void rotate(const GString *path)
{
    GString *prev = NULL, *cur = g_string_new("");
    GString *tmp;
    int i;

    for (i = MAX_ROTATE_FILENO; i > 0; i--)
    {
	g_string_printf(cur, "%s-%i", path->str, i);

	if (prev) g_rename(cur->str, prev->str);
	else	  prev = g_string_new("");

	/* swap pointers */
	tmp = prev; prev = cur; cur = tmp;
    }
    g_rename(path->str, prev->str);

    g_string_free(cur, TRUE);
    g_string_free(prev, TRUE);
}

/* Move the .diamond/NEWSCOPE file to .diamond/SCOPE */
int ls_define_scope(ls_search_handle_t handle)
{
    GString *src, *dst;
    gchar *megacookie;
    gsize len;
    char *home;
    int err;

    home = getenv("HOME");
    if (!home) {
	fprintf(stderr, "libscope: Couldn't get user's home directory!\n");
	return -1;
    }

    src = g_string_new(home);
    g_string_append(src, "/.diamond/NEWSCOPE");

    if (!g_file_test(src->str, G_FILE_TEST_IS_REGULAR))
    {
	fprintf(stderr, "libscope: There is no scope file at %s\n", src->str);
	return 0;
    }

    if (!g_file_get_contents(src->str, &megacookie, &len, NULL))
    {
	fprintf(stderr, "libscope: Couldn't read %s!\n", src->str);
	return -1;
    }

    err = ls_set_scope(handle, megacookie);

    /* make a backup copy of the loaded scope */
    if (!err) {
	/* Rotate old scope files out of the way */
	dst = g_string_new(home);
	g_string_append(dst, "/.diamond/SCOPE");
	rotate(dst);

	g_file_set_contents(dst->str, megacookie, len, NULL);

	g_string_free(dst, TRUE);
    }

    g_string_free(src, TRUE);
    g_free(megacookie);
    return err;
}

gchar **scope_split_cookies(const gchar *buf)
{
    GPtrArray *cookies = g_ptr_array_new();
    GString *cookie;
    gchar **lines;
    int i, in_cookie = 0;

    cookie = g_string_new("");
    lines = g_strsplit(buf, "\n", 0);

    for (i = 0; lines[i]; i++)
    {
	if (strcmp(lines[i], BEGIN_COOKIE) == 0)
	    in_cookie = 1;

	if (!in_cookie)
	    continue;

	g_string_append(cookie, lines[i]);
	g_string_append_c(cookie, '\n');

	if (strcmp(lines[i], END_COOKIE) != 0)
	    continue;

	g_ptr_array_add(cookies, g_strndup(cookie->str, cookie->len));
	g_string_truncate(cookie, 0);
	in_cookie = 0;
    }
    g_strfreev(lines);
    g_string_free(cookie, TRUE);

    g_ptr_array_add(cookies, NULL);
    return (gchar **)g_ptr_array_free(cookies, FALSE);
}

gchar **scope_get_servers(const gchar *cookie)
{
    struct scopecookie *scope;
    gchar **servers;

    scope = scopecookie_parse(cookie);
    if (!scope) return NULL;

    servers = g_strdupv(scope->servers);
    scopecookie_free(scope);

    return servers;
}

