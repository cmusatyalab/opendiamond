/*
 * 	Diamond (Release 1.0)
 *      A system for interactive brute-force search
 *
 *      Copyright (c) 2002-2005, Intel Corporation
 *      All Rights Reserved
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

#include "sig_calc.h"


static char const cvsid[] = "$Header$";

static const EVP_MD *md;
static int	 done_sig_init = 0;

int
sig_cal_init()
{
	/* make sure we only call once */
	if (done_sig_init) {
		return(0);
	}

	OpenSSL_add_all_digests();
	md = EVP_get_digestbyname("md5");
	if(!md) {
		perror("Unknown message digest md5");
		assert( md!= NULL);
		//exit(1);
	}
	done_sig_init = 1;
	return(0);
}

int
sig_cal(const void *buf, off_t buflen, sig_val_t *sig_val)
{
	EVP_MD_CTX mdctx;
	unsigned char *md_value;
	int md_len=0;

	assert(done_sig_init == 1);

	md_value = sig_val->sig;

	EVP_MD_CTX_init(&mdctx);
	EVP_DigestInit_ex(&mdctx, md, NULL);

	EVP_DigestUpdate(&mdctx, buf, buflen);

	EVP_DigestFinal_ex(&mdctx, md_value, &md_len);
	EVP_MD_CTX_cleanup(&mdctx);
	/*
	    printf("Digest is: ");
	    for(i = 0; i < md_len; i++)
	        printf("%x", md_value[i]);
	    printf("\n");
	*/
	return(0);
}

