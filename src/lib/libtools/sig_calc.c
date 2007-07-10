/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 3
 *
 *  Copyright (c) 2002-2005 Intel Corporation
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
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


static const EVP_MD *md;
static int      done_sig_init = 0;

int
sig_cal_init()
{
	/*
	 * make sure we only call once 
	 */
	if (done_sig_init) {
		return (0);
	}

	OpenSSL_add_all_digests();
	md = EVP_get_digestbyname("md5");
	if (!md) {
		perror("Unknown message digest md5");
		assert(md != NULL);
		// exit(1);
	}
	done_sig_init = 1;
	return (0);
}

int
sig_cal(const void *buf, off_t buflen, sig_val_t * sig_val)
{
	EVP_MD_CTX      mdctx;
	unsigned char  *md_value;
	unsigned int	md_len = 0;

	assert(done_sig_init == 1);

	md_value = sig_val->sig;

	EVP_MD_CTX_init(&mdctx);
	EVP_DigestInit_ex(&mdctx, md, NULL);

	EVP_DigestUpdate(&mdctx, buf, buflen);

	EVP_DigestFinal_ex(&mdctx, md_value, &md_len);
	EVP_MD_CTX_cleanup(&mdctx);
	/*
	 * printf("Digest is: "); for(i = 0; i < md_len; i++) printf("%x",
	 * md_value[i]); printf("\n"); 
	 */
	return (0);
}


int
sig_cal_str(const char *buf, sig_val_t * sig_val)
{
	off_t           len = strlen(buf);
	int             err;

	err = sig_cal(buf, len, sig_val);
	return (err);
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

unsigned long
sig_hash(sig_val_t * sig)
{
	int             i;
	unsigned long   v = 0;

	for (i = 0; i < SIG_SIZE; i++) {
		v = ((unsigned long) sig->sig[i]) + (v << 6) + (v << 16) - v;
	}

	return (v);
}


int
sig_match(sig_val_t * sig1, sig_val_t * sig2)
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
