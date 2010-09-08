/*
 * miniRPC - Simple TCP RPC library
 *
 * Copyright (C) 2007 Carnegie Mellon University
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
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
