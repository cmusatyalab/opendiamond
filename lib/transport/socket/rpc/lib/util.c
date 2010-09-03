/*
 * miniRPC - TCP RPC library with asynchronous operations
 *
 * Copyright (C) 2007-2008 Carnegie Mellon University
 *
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU Lesser General Public License version 2.1, as shown in
 * the file COPYING.
 */

#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>
#include <pthread.h>
#define MINIRPC_INTERNAL
#include "internal.h"

static void _mrpc_init(void)
{
	if (!g_thread_supported())
		g_thread_init(NULL);
	mrpc_event_threadlocal_init();
}

void mrpc_init(void)
{
	static pthread_once_t started = PTHREAD_ONCE_INIT;

	pthread_once(&started, _mrpc_init);
}

int set_nonblock(int fd)
{
	int flags;

	flags=fcntl(fd, F_GETFL);
	if (flags == -1)
		return errno;
	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK))
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

int block_signals(void)
{
	sigset_t sigs;

	if (sigfillset(&sigs))
		return errno;
	return pthread_sigmask(SIG_SETMASK, &sigs, NULL);
}

void assert_callback_func(void *ignored)
{
	assert(0);
}

exported const char *mrpc_strerror(mrpc_status_t status)
{
	enum mrpc_status_codes code=status;

	switch (code) {
	case MINIRPC_OK:
		return "Success";
	case MINIRPC_PENDING:
		/* Not really an error code, and the application should never
		   receive this as a return value */
		break;
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
