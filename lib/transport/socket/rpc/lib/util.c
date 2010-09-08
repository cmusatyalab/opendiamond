/*
 * miniRPC - Simple TCP RPC library
 *
 * Copyright (C) 2007-2008 Carnegie Mellon University
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#include <fcntl.h>
#include <errno.h>
#define MINIRPC_INTERNAL
#include "internal.h"

int set_blocking(int fd)
{
	int flags;

	flags=fcntl(fd, F_GETFL);
	if (flags == -1)
		return errno;
	if (fcntl(fd, F_SETFL, flags & ~O_NONBLOCK))
		return errno;
	return 0;
}

int set_cloexec(int fd)
{
	int flags;

	flags=fcntl(fd, F_GETFD);
	if (flags == -1)
		return errno;
	if (fcntl(fd, F_SETFD, flags | FD_CLOEXEC))
		return errno;
	return 0;
}

exported const char *mrpc_strerror(mrpc_status_t status)
{
	enum mrpc_status_codes code=status;

	switch (code) {
	case MINIRPC_OK:
		return "Success";
	case MINIRPC_ENCODING_ERR:
		return "Serialization error";
	case MINIRPC_PROCEDURE_UNAVAIL:
		return "Procedure not available at this time";
	case MINIRPC_INVALID_ARGUMENT:
		return "Invalid argument";
	case MINIRPC_INVALID_PROTOCOL:
		return "Operation does not match connection role";
	case MINIRPC_NETWORK_FAILURE:
		return "Connection failure";
	}
	if (code < 0)
		return "Unknown error";
	else
		return "Application-specific error";
}
