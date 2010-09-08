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

#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include "common.h"

static int get_max_files(void)
{
	struct rlimit rlim;

	if (getrlimit(RLIMIT_NOFILE, &rlim))
		die("Couldn't get system fd limit");
	return rlim.rlim_cur;
}

int main(int argc, char **argv)
{
	struct mrpc_connection *sconn;
	struct mrpc_connection *conn;
	int fd;
	int max;
	pid_t pid;
	int status;
	int cleanup;
	int a, b;

	cleanup=(argc == 2 && !strcmp(argv[1], "cleanup"));
	max=get_max_files();
	for (fd=3; fd<max; fd++) {
		if (cleanup) {
			if (fcntl(fd, F_GETFD) != -1)
				die("fd %d still open after exec", fd);
		} else {
			close(fd);
		}
	}
	if (cleanup)
		return 0;

	get_conn_pair(&a, &b);
	if (mrpc_conn_create(&sconn, proto_server, a, NULL))
		die("Couldn't allocate conn set");
	sync_server_set_ops(sconn);
	start_monitored_dispatcher(sconn);

	if (mrpc_conn_create(&conn, proto_client, b, NULL))
		die("Couldn't allocate conn set");
	sync_client_set_ops(conn);
	sync_client_run(conn);

	pid=fork();
	if (pid) {
		waitpid(pid, &status, 0);
		if (!WIFEXITED(status) || WEXITSTATUS(status))
			return 1;
	} else {
		if (execlp(argv[0], argv[0], "cleanup", (char*)NULL))
			die("Couldn't re-exec test");
	}

	sync_client_run(conn);
	mrpc_conn_close(conn);
	mrpc_conn_free(conn);
	dispatcher_barrier();
	return 0;
}
