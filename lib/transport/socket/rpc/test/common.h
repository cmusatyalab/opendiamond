/*
 * miniRPC - Simple TCP RPC library
 *
 * Copyright (C) 2007-2010 Carnegie Mellon University
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#define FAILURE_TIMEOUT 15	/* seconds */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <minirpc/minirpc.h>
#include "proto_client.h"
#include "proto_server.h"

/* common.c */
void _message(const char *file, int line, const char *func, const char *fmt,
			...);
#define message(args...) _message(__FILE__, __LINE__, __func__, args)
#define die(args...) do {message(args); abort();} while (0)
void start_monitored_dispatcher(struct mrpc_connection *conn);
void get_conn_pair(int *a, int *b);
void dispatcher_barrier(void);
#define expect(cmd, result) do {					\
		int _ret=cmd;						\
		int _expected=result;					\
		if (_ret != _expected)					\
			die("%s returned %d (%s), expected %d (%s)",	\
						#cmd, _ret,		\
						strerror(_ret),		\
						_expected,		\
						strerror(_expected));	\
	} while (0)

/* client_sync.c */
void loop_int_sync(struct mrpc_connection *conn);
void check_int_sync(struct mrpc_connection *conn);
void error_sync(struct mrpc_connection *conn);
void trigger_callback_sync(struct mrpc_connection *conn);
void invalidate_sync(struct mrpc_connection *conn);
mrpc_status_t send_buffer_sync(struct mrpc_connection *conn);
mrpc_status_t recv_buffer_sync(struct mrpc_connection *conn);
void sync_client_set_ops(struct mrpc_connection *conn);
void sync_client_run(struct mrpc_connection *conn);

/* server_sync.c */
void sync_server_set_ops(struct mrpc_connection *conn);

#endif
