/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 4
 *
 *  Copyright (c) 2002-2005 Intel Corporation
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
 *  Copyright (c) 2007-2009 Carnegie Mellon University
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 *
 */

#include <errno.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <glib.h>

#include "sig_calc.h"
#include "sig_calc_priv.h"
#include "g_checksum_compat.h"

void
sig_cal_vec(const struct ciovec *iov, int iovcnt, sig_val_t *signature)
{
	GChecksum *mdctx;
	unsigned int len = SIG_SIZE;
	int i;

	mdctx = g_checksum_new(G_CHECKSUM_MD5);
	assert(mdctx);

	for (i = 0; i < iovcnt; i++)
	    if (iov[i].iov_len)
		g_checksum_update(mdctx, iov[i].iov_base, iov[i].iov_len);

	g_checksum_get_digest(mdctx, signature->sig, &len);
	assert(len == SIG_SIZE);

	g_checksum_free(mdctx);
}

int
sig_cal(const void *buf, off_t buflen, sig_val_t * sig_val)
{
	struct ciovec iov = {
		.iov_base = buf,
		.iov_len = buflen,
	};
	sig_cal_vec(&iov, 1, sig_val);
	return 0;
}

int
sig_cal_str(const char *buf, sig_val_t * sig_val)
{
	struct ciovec iov = {
		.iov_base = buf,
		.iov_len = strlen(buf),
	};
	sig_cal_vec(&iov, 1, sig_val);
	return 0;
}

char           *
sig_string(sig_val_t * sig_val)
{
	char           *new_str;
	char           *cp;
	int             i;

	new_str = (char *) malloc(2 * SIG_SIZE + 1);
	if (new_str == NULL) {
		/*
		 * XXX log?? 
		 */
		return (NULL);
	}
	cp = new_str;
	for (i = 0; i < SIG_SIZE; i++) {
		sprintf(cp, "%02X", sig_val->sig[i]);
		cp += 2;
	}

	/*
	 * terminate the string 
	 */
	*cp = '\0';

	return (new_str);
}

void
string_to_sig(char *string, sig_val_t * sig_val)
{
	int             i;
	char		tmp_str[4];

	for (i = 0; i < SIG_SIZE; i++) {
		tmp_str[0] = *string++;
		tmp_str[1] = *string++;
		tmp_str[2] = 0;

		sig_val->sig[i] = strtol(tmp_str, NULL, 16);
	}
}

unsigned int
sig_hash(const sig_val_t * sig)
{
	unsigned int v =
	    (((unsigned int)sig->sig[0]) << 24) +
	    (((unsigned int)sig->sig[1]) << 16) +
	    (((unsigned int)sig->sig[2]) << 8) +
	    (((unsigned int)sig->sig[3]));

	return v;
}


int
sig_match(const sig_val_t * sig1, const sig_val_t * sig2)
{
	return memcmp(sig1, sig2, sizeof(sig_val_t)) == 0;
}

void
sig_clear(sig_val_t * sig)
{
	memset(sig, 0, sizeof(sig_val_t));
}
