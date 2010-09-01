/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 4
 *
 *  Copyright (c) 2002-2005 Intel Corporation
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
 *  Copyright (c) 2006-2008 Carnegie Mellon University
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#ifndef _SOCKET_TRANS_H_
#define _SOCKET_TRANS_H_

#include <stdint.h>
#include "diamond_consts.h"
#include "sig_calc.h"
#include "rpc_preamble_xdr.h"


/* default number of credits to start with on the server */
#define DEFAULT_QUEUE_LEN	10


ssize_t writen(int fd, const void *vptr, size_t n);

#endif /* _SOCKET_TRANS_H_ */
