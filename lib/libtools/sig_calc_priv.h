/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 4
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

#ifndef _SIG_CALC_PRIV_H_
#define _SIG_CALC_PRIV_H_


/*
 * Compute the signature for a scatter-gather list of byte ranges
 */
/* 'const'-version of struct iovec (sys/uio.h) */
struct ciovec {
	const void *iov_base;
	size_t iov_len;
};
void sig_cal_vec(const struct ciovec *iov, int iovcnt, sig_val_t *signature);

/*
 * Compute the signature of a single range of bytes.
 */
int sig_cal(const void *buf, off_t buflen, sig_val_t * sig);

int sig_cal_str(const char *buf, sig_val_t * sig);


unsigned int sig_hash(const sig_val_t *sig);

int sig_match(const sig_val_t *sig1, const sig_val_t *sig2);

void sig_clear(sig_val_t *sig);


#endif
