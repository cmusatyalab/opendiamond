/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 2
 *
 *  Copyright (c) 2007 Carnegie Mellon University
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#ifndef XDR_SHIM_H
#define XDR_SHIM_H

#include <rpc/rpc.h>
#include <tirpc/rpc/types.h>

/*
 * The following defines a missing function that rpcgen calls in
 * generated code.  It is mysteriously missing from TI-RPC, but may
 * have been added to Sun RPC after the two forked. It handles 64-bit
 * datatypes.
 */

#define xdr_quad_t xdr_longlong_t
void opendiamond_prog_2(struct svc_req *rqstp, register SVCXPRT *transp);

#endif
