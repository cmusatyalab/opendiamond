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

#include <string.h>
#include "uri_util.h"

struct uri {
    gchar *scheme;
    gchar *netloc;
    gchar *path;
    gchar *params;
    gchar *query;
    gchar *fragment;

    gchar *alloc; /* holds ptr to area that should be freed */
    int relpath;  /* for relative urls */
};

/* Break URL into separate components as recommended by RFC1808 */
static struct uri *uri_parse(const gchar *url)
{
    struct uri *URL = g_new0(struct uri, 1);
    gchar *ptr, *copy;

    URL->alloc = copy = g_strdup(url);

    /* fragment; substring after left-most '#' */
    ptr = strchr(copy, '#');
    if (ptr) {
	*(ptr++) = '\0';
	URL->fragment = ptr;
    }

    /* scheme; alphanumeric, '+', '.', '-', characters leading up to ':' */
    for (ptr = copy; *ptr; ptr++)
	if (!g_ascii_isalnum(*ptr) && *ptr != '+' && *ptr != '.' && *ptr != '-')
	    break;
    if (*ptr == ':') {
	URL->scheme = copy;
	*(ptr++) = '\0';
	copy = ptr;
    }

    /* network location/login; from "//" to next '/' */
    if (*copy == '/') {
	copy++;
	if (*copy == '/') {
	    URL->netloc = ++copy;
	    ptr = strchr(copy, '/');
	    if (!ptr) return URL;

	    *(ptr++) = '\0';
	    copy = ptr;
	}
    } else
	URL->relpath = 1;

    /* query information; after left-most '?' */
    ptr = strchr(copy, '?');
    if (ptr) {
	*(ptr++) = '\0';
	URL->query = ptr;
    }

    /* parameters; after left-most ';' */
    ptr = strchr(copy, ';');
    if (ptr) {
	*(ptr++) = '\0';
	URL->params = ptr;
    }

    /* path */
    URL->path = copy;
    return URL;
}

/* Format uri as a newly allocate URL string */
static gchar *uri_format(struct uri *url)
{
    gchar *result[13];
    int i = 0;

    if (url->scheme) {
	result[i++] = url->scheme;
	result[i++] = ":";
    }
    if (url->netloc) {
	result[i++] = "//";
	result[i++] = url->netloc;
    }
    if (url->path) {
	if (!url->relpath)
	    result[i++] = "/";
	result[i++] = url->path;
    }
    if (url->params) {
	result[i++] = ";";
	result[i++] = url->params;
    }
    if (url->query) {
	result[i++] = "?";
	result[i++] = url->query;
    }
    if (url->fragment) {
	result[i++] = "#";
	result[i++] = url->fragment;
    }
    result[i] = NULL;
    return g_strjoinv("", result);
}

/* Release uri data structure */
static void uri_free(struct uri *url)
{
    if (url)
	g_free(url->alloc);
    g_free(url);
}

/* Resolve relative URL according to RFC 1808 section 4 */
gchar *uri_normalize(const gchar *url, const gchar *base_url)
{
    struct uri *rel, *base;
    gchar **comps, **rel_c;
    gchar *result, *path = NULL;
    int i, j;

    /* step 1 */
    if (!base_url || !*base_url)
	return g_strdup(url);

    /* step 2a */
    if (!url || !*url)
	return g_strdup(base_url);

    rel = uri_parse(url);
    base = uri_parse(base_url);

    /* step 2b */
    if (rel->scheme) goto done;

    /* step 2c */
    rel->scheme = base->scheme;

    /* step 3 */
    if (rel->netloc) goto done;
    rel->netloc = base->netloc;

    /* step 4 */
    if (!rel->relpath) goto done;

    /* step 5 */
    rel->relpath = 0;
    if (!rel->path) {
	rel->path = base->path;

	/* step 5a */
	if (rel->params) goto done;
	rel->params = base->params;

	/* step 5b */
	if (rel->query) goto done;
	rel->query = base->query;
	goto done;
    }

    /* step 6 */
    comps = g_strsplit(base->path, "/", -1);
    rel_c = g_strsplit(rel->path, "/", -1);
    for (i = 0; comps[i]; i++) /* count */;
    for (j = 0; rel_c[j]; j++) /* count */;

    /* grow the array of pathname components */
    comps = g_renew(gchar *, comps, i+j);

    /* drop the last component of the base url */
    if (i > 0)
	g_free(comps[--i]);

    /* append relative url path components */
    for (j = 0; rel_c[j]; j++) {
	/* step 6a, 6b */
	if (strcmp(rel_c[j], ".") == 0)
	    g_free(rel_c[j]);

	/* step 6c, 6d */
	else if (strcmp(rel_c[j], "..") == 0)
	{
	    if (i > 0) {
		g_free(comps[--i]);
	    }
	    g_free(rel_c[j]);
	}
	else
	    comps[i++] = rel_c[j];
    }
    g_free(rel_c);

    /* and recombine as a single path */
    comps[i] = NULL;
    rel->path = path = g_strjoinv("/", comps);
    g_strfreev(comps);

done: /* step 7 */
    result = uri_format(rel);

    uri_free(base);
    uri_free(rel);
    g_free(path);
    return result;
}

