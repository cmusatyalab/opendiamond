/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 4
 *
 *  Copyright (c) 2002-2005 Intel Corporation
 *  Copyright (c) 2007 Carnegie Mellon University
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#ifndef	_SIG_CALC_H_
#define	_SIG_CALC_H_

#define	SIG_SIZE	16

#include <diamond_features.h>

typedef	struct sig_val {
	unsigned char 	sig[SIG_SIZE];
} sig_val_t;

/* 
 * return string holding ascii value of the signature,
 * This memory is malloc'ed so called needs to free when they are don.
 */
diamond_public
char * sig_string(sig_val_t *sig);

diamond_public
void string_to_sig(char *string, sig_val_t * sig_val);


#endif	/* !_SIG_CALC_H_ */


