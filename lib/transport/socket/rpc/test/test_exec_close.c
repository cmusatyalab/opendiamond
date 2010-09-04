/*
 * miniRPC - TCP RPC library with asynchronous operations
 *
 * Copyright (C) 2007-2010 Carnegie Mellon University
 *
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU Lesser General Public License version 2.1, as shown in
 * the file COPYING.
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
	struct mrpc_conn_set *sset;
	struct mrpc_conn_set *cset;
	struct mrpc_connection *sconn;
	struct mrpc_connection *conn;
	int ret;
	int fd;
	int max;
	pid_t pid;
	int status;
	int cleanup;

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

	if (mrpc_conn_set_create(&sset, proto_server, NULL))
		die("Couldn't allocate conn set");
	start_monitored_dispatcher(sset);
	mrpc_set_disconnect_func(sset, disconnect_normal);

	if (mrpc_conn_set_create(&cset, proto_client, NULL))
		die("Couldn't allocate conn set");
	mrpc_set_disconnect_func(cset, disconnect_user);

	ret=mrpc_conn_create(&sconn, sset, NULL);
	if (ret)
		die("%s", strerror(ret));
	sync_server_set_ops(sconn);
	ret=mrpc_conn_create(&conn, cset, NULL);
	if (ret)
		die("%s", strerror(ret));
	bind_conn_pair(sconn, conn);

	start_monitored_dispatcher(cset);
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
	mrpc_conn_unref(conn);
	mrpc_conn_set_unref(cset);
	mrpc_conn_set_unref(sset);
	expect_disconnects(1, 1, 0);
	return 0;
}
