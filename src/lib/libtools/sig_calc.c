/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 4
 *
 *  Copyright (c) 2002-2005 Intel Corporation
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
 *  Copyright (c) 2007 Carnegie Mellon University
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#include <errno.h>
#include <openssl/evp.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "sig_calc.h"
#include "sig_calc_priv.h"


static const EVP_MD *md;

static int
sig_cal_init(void)
{
	/*
	 * make sure we only call once
	 */
	if (md != NULL) {
		return (0);
	}

	OpenSSL_add_all_digests();
	md = EVP_get_digestbyname("md5");
	if (!md) {
		perror("Unknown message digest md5");
		assert(md != NULL);
		// exit(1);
	}

	/* make sure the digest will fit in our sig_val_t buffer */
	assert(EVP_MD_size(md) == SIG_SIZE);
	return (0);
}

void
sig_cal_vec(const struct ciovec *iov, int iovcnt, sig_val_t *signature)
{
	EVP_MD_CTX mdctx;
	int i;

	sig_cal_init();

	EVP_MD_CTX_init(&mdctx);
	EVP_DigestInit_ex(&mdctx, md, NULL);

	for (i = 0; i < iovcnt; i++)
		EVP_DigestUpdate(&mdctx, iov[i].iov_base, iov[i].iov_len);

	EVP_DigestFinal_ex(&mdctx, signature->sig, NULL);
	EVP_MD_CTX_cleanup(&mdctx);
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
	if (memcmp(sig1, sig2, sizeof(sig_val_t)) == 0) {
		return (1);
	} else {
		return (0);
	}
}

void
sig_clear(sig_val_t * sig)
{
	memset(sig, 0, sizeof(sig_val_t));

}
