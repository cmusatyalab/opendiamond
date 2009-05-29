/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 4
 *
 *  Copyright (c) 2002-2007 Intel Corporation
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
 *  Copyright (c) 2006-2009 Carnegie Mellon University
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <dirent.h>
#include <glib.h>
#include "diamond_consts.h"
#include "diamond_types.h"
#include "lib_tools.h"
#include "obj_attr.h"
#include "lib_odisk.h"
#include "lib_sstub.h"
#include "lib_filterexec.h"
#include "search_state.h"


/*
 * The default behaviors are to create a new daemon at startup time
 * and to fork whenever a new connection is established.
 */
int             do_daemon = 1;
int             do_fork = 1;
int             not_silent = 0;
int             bind_locally = 0;

static void
usage(void)
{
	fprintf(stdout, "adiskd -[abcdlhins] \n");
	fprintf(stdout, "\t -b run background tasks \n");
	fprintf(stdout, "\t -d do not run adisk as a daemon \n");
	fprintf(stdout, "\t -h get this help message \n");
	fprintf(stdout, "\t -i allow background to run when not idle\n");
	fprintf(stdout, "\t -l only listen on localhost \n");
	fprintf(stdout, "\t -n do not fork for a  new connection \n");
	fprintf(stdout, "\t -s do not close stderr on fork \n");
}


int
main(int argc, char **argv)
{
	int             err;
	void           *cookie;
	sstub_cb_args_t cb_args;
	int             c;

	/*
	 * Parse any of the command line arguments.
	 */

	while (1) {
		c = getopt(argc, argv, "abdhilns");
		if (c == -1) {
			break;
		}
		switch (c) {
		case 'd':
			do_daemon = 0;
			break;

		case 'h':
			usage();
			exit(0);
			break;

		case 'l':
			bind_locally = 1;
			break;

		case 'n':
			/* this is a debugging mode where we don't
			 * want extra processes so diable most of them.
			 */
			do_fork = 0;
			do_daemon = 0;
			break;

		case 's':
			not_silent = 1;
			break;


		default:
			usage();
			exit(1);
			break;
		}
	}



	/*
	 * make this a daemon by the appropriate call 
	 */
	if (do_daemon) {
		err = daemon(1, not_silent);
		if (err != 0) {
			perror("daemon call failed !! \n");
			exit(1);
		}
	}


	cb_args.new_conn_cb = search_new_conn;
	cb_args.close_conn_cb = search_close_conn;
	cb_args.start_cb = search_start;
	cb_args.stop_cb = search_stop;

	cb_args.set_fspec_cb = search_set_spec;
	cb_args.set_fobj_cb = search_set_obj;

	cb_args.terminate_cb = search_term;
	cb_args.release_obj_cb = search_release_obj;
	cb_args.get_char_cb = search_get_char;
	cb_args.get_stats_cb = search_get_stats;
	cb_args.setlog_cb = search_setlog;
	cb_args.rleaf_cb = search_read_leaf;
	cb_args.wleaf_cb = search_write_leaf;
	cb_args.lnode_cb = search_list_nodes;
	cb_args.lleaf_cb = search_list_leafs;
	cb_args.clear_scope_cb = search_clear_scope;
	cb_args.set_scope_cb = search_set_scope;
	cb_args.set_gid_cb = search_set_gid;
	cb_args.set_blob_cb = search_set_blob;
	cb_args.set_user_state_cb = search_set_user_state;
	cb_args.get_session_vars_cb = search_get_session_vars;
	cb_args.set_session_vars_cb = search_set_session_vars;
	cb_args.reexecute_filters = search_reexecute_filters;

	cookie = sstub_init(&cb_args, bind_locally);
	if (cookie == NULL) {
		fprintf(stderr, "Unable to initialize the communications\n");
		exit(1);
	}


	/*
	 * We provide the main thread for the listener.  This
	 * should never return.
	 */
	while (1) {
		sstub_listen(cookie);

		/*
		 * We need to do a periodic wait. To get rid
		 * of all the zombies.  The posix spec appears to
		 * be fuzzy on the behavior of setting sigchild
		 * to ignore, so we will do a periodic nonblocking
		 * wait to clean up the zombies.
		 */
		waitpid(-1, NULL, WNOHANG | WUNTRACED);
	}

	exit(0);
}
