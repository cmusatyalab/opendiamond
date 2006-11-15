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

#ifndef	_SIG_CALC_H_
#define	_SIG_CALC_H_

#define	SIG_SIZE	16

typedef	struct sig_val {
	unsigned char 	sig[SIG_SIZE];
} sig_val_t;

/*
 * Must be called once before using the library.
 */
int sig_cal_init();

/*
 * Compute the signature of a single range of bytes.
 */
int sig_cal(const void *buf, off_t buflen, sig_val_t * sig);

int sig_cal_str(const char *buf, sig_val_t * sig);

/* 
 * return string holding ascii value of the signature,
 * This memory is malloc'ed so called needs to free when they are don.
 */
char * sig_string(sig_val_t *sig);


void string_to_sig(char *string, sig_val_t * sig_val);

unsigned long sig_hash(sig_val_t *sig);

int sig_match(sig_val_t *sig1, sig_val_t *sig2);

void sig_clear(sig_val_t *sig);


#endif	/* !_SIG_CALC_H_ */


