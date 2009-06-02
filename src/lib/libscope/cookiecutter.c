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

#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#include <uuid/uuid.h>
#include <glib.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include "scope_priv.h"

#define SUMM "Generates an OpenDiamond(r) cookie. If no --scopeurl argument has been given it will\nread the scope specification from stdin."

static gchar	**servers;
static gint	expiry = 3600;
static gboolean	verbose = FALSE;
static gchar	*keyfile;
static gchar	**scopeurls;

static GOptionEntry options[] = {
    { "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, "Increase verbosity", NULL},
    { "server", 's', 0, G_OPTION_ARG_STRING_ARRAY, &servers, "Specify server that accepts the cookie (can be repeated)", "host"},
    { "expire", 'e', 0, G_OPTION_ARG_INT, &expiry, "Time in seconds until cookie expires (3600)", NULL},
    { "key", 'k', 0, G_OPTION_ARG_FILENAME, &keyfile, "X509 private key ($HOME/.diamond/key.pem)", NULL},
    { "scopeurl", 'u', 0, G_OPTION_ARG_STRING_ARRAY, &scopeurls, "URL from which scopelist can be retrieved (can be repeated)", "host"},
    { .long_name=NULL }
};

static void string_hex_encode(GString *buf)
{
    const gchar *hx = "0123456789abcdef";
    gsize new_len = buf->len * 2;
    guchar *c, *p;
    unsigned int i;

    if (buf->allocated_len <= new_len)
    {
	buf->allocated_len = new_len + 1;
	buf->str = g_renew(gchar, buf->str, new_len + 1);
    }

    c = (guchar *)&buf->str[buf->len-1];
    p = (guchar *)&buf->str[new_len];
    *(p--) = '\0';

    for (i = buf->len; i > 0; i--, c--) {
	*(p--) = hx[*c & 0xf];
	*(p--) = hx[*c >> 4];
    }
    buf->len = new_len;
}

static void string_base64_encode(GString *buf)
{
    gchar *tmp;
    gsize i, n, len;

    tmp = g_base64_encode((void *)buf->str, buf->len);
    len = strlen(tmp); n = 64;

    g_string_assign(buf, "");
    for (i = 0; len; i += n, len -= n)
    {
	if (len < n) n = len;
	g_string_append_len(buf, &tmp[i], n);
	g_string_append_c(buf, '\n');
    }
    g_free(tmp);
}

static gchar *scopecookie_create(GTimeVal *expires, gchar **servers,
				 const gchar *scope, gsize scopelen,
				 gnutls_x509_privkey_t *key)
{
    GString *scopecookie, *tmp;
    gint version = SCOPECOOKIE_VERSION;
    uuid_t serial;
    gchar uuid[37], *p;
    gnutls_datum_t buf;
    gsize len;

    /* construct cookie */
    scopecookie = g_string_new("");
    g_string_append_printf(scopecookie, "Version: %d\n", version);

    /* unique serial */
    uuid_generate_random(serial);
    uuid_unparse(serial, uuid);
    g_string_append_printf(scopecookie, "Serial: %s\n", uuid);

    /* signing key identifier */
    gnutls_x509_privkey_get_key_id(*key, 0, NULL, &len);
    tmp = g_string_sized_new(len+1);
    gnutls_x509_privkey_get_key_id(*key, 0, (void *)tmp->str, &len);
    tmp->len = len;

    string_hex_encode(tmp);
    g_string_append_printf(scopecookie, "KeyId: %s\n", tmp->str);
    g_string_free(tmp, TRUE);

    /* expiration time */
    p = g_time_val_to_iso8601(expires);
    g_string_append_printf(scopecookie, "Expires: %s\n", p);
    g_free(p);

    /* list of servers */
    p = g_strjoinv(";", servers);
    g_string_append_printf(scopecookie, "Servers: %s\n", p);
    g_free(p);

    /* append actual scope definition */
    g_string_append_c(scopecookie, '\n');
    g_string_append_len(scopecookie, scope, scopelen);

    /* sign cookie */
    buf.data = (void *)scopecookie->str;
    buf.size = scopecookie->len;

    gnutls_x509_privkey_sign_data(*key, GNUTLS_DIG_SHA1, 0, &buf, NULL, &len);
    tmp = g_string_sized_new(len+1);
    gnutls_x509_privkey_sign_data(*key, GNUTLS_DIG_SHA1, 0, &buf, tmp->str, &len);
    tmp->len = len;

    string_hex_encode(tmp);
    g_string_append_c(tmp, '\n');
    g_string_prepend_len(scopecookie, tmp->str, tmp->len);
    g_string_free(tmp, TRUE);

    if (verbose) {
	fputs(scopecookie->str, stderr);
	fputc('\n', stderr);
    }

    /* base64 encode cookie contents */
    string_base64_encode(scopecookie);

    /* add begin/end cookie markers */
    g_string_prepend_c(scopecookie, '\n');
    g_string_prepend(scopecookie, BEGIN_COOKIE);
    g_string_append(scopecookie, END_COOKIE);
    g_string_append_c(scopecookie, '\n');

    return g_string_free(scopecookie, FALSE);
}

int main(int argc, char **argv)
{
    GOptionContext *context;
    GTimeVal expires;
    GError *error = NULL;
    GIOChannel *gio;
    gchar *scope, *cookie;
    gnutls_datum_t buf;
    gnutls_x509_privkey_t key;
    gsize len;
    int rc;

    context = g_option_context_new("- generate scope cookie");
    g_option_context_set_summary(context, SUMM);
    g_option_context_add_main_entries(context, options, NULL);
    g_option_context_parse(context, &argc, &argv, &error);
    g_option_context_free(context);

    if (!keyfile) {
	GString *tmp = g_string_new(getenv("HOME"));
	g_string_append(tmp, "/.diamond/key.pem");
	keyfile = g_string_free(tmp, FALSE);
    }

    if (!error && !servers)
	g_set_error(&error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
		    "Specify at least one or more servers");

    if (!error)
	g_file_get_contents(keyfile, (char **)&buf.data, &buf.size, &error);

    if (error) {
	fprintf(stderr, "Error: %s\n\n  For more information, '%s --help'\n\n",
		error->message, g_get_prgname());
	exit(0);
    }

    gnutls_global_init();
    gnutls_x509_privkey_init(&key);
    rc = gnutls_x509_privkey_import(key, &buf, GNUTLS_X509_FMT_PEM);
    g_free(buf.data);

    if (rc < 0) {
	fprintf(stderr, "Unabled to read X509 private key from \"%s\": %s\n",
		keyfile, gnutls_strerror(rc));
	exit(0);
    }

    if (scopeurls) {
	scope = g_strjoinv("\n", scopeurls);
	len = strlen(scope);
	g_strfreev(scopeurls);
    } else {
	/* read the scope description from stdin */
	gio = g_io_channel_unix_new(STDIN_FILENO);
	g_io_channel_read_to_end(gio, &scope, &len, NULL);
	g_io_channel_unref(gio);
    }

    /* get expiration time */
    g_get_current_time(&expires);
    expires.tv_sec += expiry;

    cookie = scopecookie_create(&expires, servers, scope, len, &key);
    fputs(cookie, stdout);

    g_free(cookie);
    g_free(scope);
    g_strfreev(servers);
    g_free(keyfile);

    gnutls_x509_privkey_deinit(key);
    gnutls_global_deinit();
    return 0;
}

