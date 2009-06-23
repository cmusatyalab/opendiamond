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
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "diamond_features.h"
#include "dconfig_priv.h"
#include "scope_priv.h"

#define CERT_FILE "CA.pem"

static int scope_x509_import_certificates(gnutls_x509_crt_t **certs)
{
    gnutls_datum_t buf;
    unsigned int ncerts = 0;
    gboolean ret;
    int n;
    gchar *certfile;

    certfile = dconf_get_certfile();
    if (!certfile) {
	fprintf(stderr, "No certificate file specified in Diamond config.\n");
	return 0;
    }

    /* read certificate file */
    ret = g_file_get_contents(certfile, (gchar **)&buf.data, &buf.size, NULL);
    g_free(certfile);
    if (!ret) return 0;

    /* import certificates */
    gnutls_x509_crt_list_import(NULL, &ncerts, &buf, GNUTLS_X509_FMT_PEM,
				GNUTLS_X509_CRT_LIST_IMPORT_FAIL_IF_EXCEED);

    *certs = g_new(gnutls_x509_crt_t, ncerts);

    n = gnutls_x509_crt_list_import(*certs,&ncerts,&buf,GNUTLS_X509_FMT_PEM,0);
    g_free(buf.data);
    return n;
}

static int scope_x509_validate_signature(const GString *keyid, const GString *sig,
					 const void *data, gsize data_len)
{
    gnutls_datum_t s, b;
    gnutls_x509_crt_t *certs = NULL;
    gchar *tmp = NULL;
    gsize len = 0;
    int i, n, rc;

    n = scope_x509_import_certificates(&certs);

    /* try to find a matching certificate */
    for (i = 0; i < n;) {
	rc = gnutls_x509_crt_get_key_id(certs[i], 0, (void *)tmp, &len);
	if (rc == GNUTLS_E_SHORT_MEMORY_BUFFER) {
	    tmp = g_renew(char, tmp, len);
	    continue; /* retry */
	}
	if (rc == 0 && len == keyid->len && memcmp(tmp, keyid->str, len) == 0)
	    break;
	i++;
    }
    g_free(tmp);

    if (i >= n) { /* n can be -1 when import fails! */
	fprintf(stderr, "Unable to find certificate for cookie signer\n");
	rc = -1;
	goto out;
    }

    s.data = (void *)sig->str;
    s.size = sig->len;

    b.data = (void *)data;
    b.size = data_len;

    if (!gnutls_x509_crt_verify_data(certs[i], 0, &b, &s))
    {
	fprintf(stderr, "Cookie signature is incorrect\n");
	rc = -1;
    }

out: /* cleanup */
    for (i = 0; i < n; i++)
	gnutls_x509_crt_deinit(certs[i]);
    g_free(certs);

    return rc;
}

int scopecookie_validate(struct scopecookie *scope)
{
    time_t now = time(NULL);
    GString *sig;
    gchar *tmp;
    gsize data_len;
    char **serverids;
    unsigned int i, j;
    int rc;
    static int initialized = 0;

    /* this is not thread-safe */
    if (!initialized) {
	gnutls_global_init();
	initialized = 1;
    }

    if (!scope) {
	fprintf(stderr, "No cookie found\n");
	return -1;
    }

    if (scope->version != SCOPECOOKIE_VERSION) {
	fprintf(stderr, "Cookie has unknown version %d\n", scope->version);
	//return -1;
    }

    if (scope->expires <= now) {
	fprintf(stderr, "Cookie expired as of %s\n", ctime(&scope->expires));
	return -1;
    }

    /* extract signature */
    tmp = g_strstr_len(scope->rawdata, scope->rawlen, "\n");
    if (!tmp) {
	fprintf(stderr, "Unable to find cookie signature\n");
	return -1;
    }

    /* check if the scope cookie is addressed to one of our fqdn names */
    serverids = dconf_get_serverids();
    for (i = 0; scope->servers[i]; i++) {
	for (j = 0; serverids[j]; j++) {
	    if (strcmp(scope->servers[i], serverids[j]) == 0)
		break;
	}
	if (serverids[j])
	    break;
    }
    if (!scope->servers[i]) {
	fprintf(stderr, "Unable to find matching server name\n");
	return -1;
    }

    sig = g_string_new_len(scope->rawdata, tmp - scope->rawdata);
    string_hex_decode(sig);
    data_len = scope->rawdata + scope->rawlen - &tmp[1];

    rc = scope_x509_validate_signature(scope->keyid, sig, &tmp[1], data_len);

    g_string_free(sig, TRUE);
    return rc;
}

