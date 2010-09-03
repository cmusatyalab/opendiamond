/*
 * miniRPC - TCP RPC library with asynchronous operations
 *
 * Copyright (C) 2007 Carnegie Mellon University
 *
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU Lesser General Public License version 2.1, as shown in
 * the file COPYING.
 */

struct mrpc_header {
	unsigned sequence;
	int status;
	int cmd;
	unsigned datalen;
};

#ifdef RPC_HDR
%#define MINIRPC_HEADER_LEN	16
#endif
