/*
 * 	Diamond (Release 1.0)
 *      A system for interactive brute-force search
 *
 *      Copyright (c) 2002-2005, Intel Corporation
 *      All Rights Reserved
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#include <pthread.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <dirent.h>
#include "diamond_consts.h"
#include "diamond_types.h"
#include "obj_attr.h"
#include "lib_odisk.h"
//XXX#include "lib_searchlet.h"
#include "lib_dctl.h"
#include "lib_sstub.h"
#include "lib_filterexec.h"
#include "ring.h"
#include "search_state.h"

static char const rcsid[] = "$Id$";



/*
 * The default behaviors are to create a new daemon at startup time
 * and to fork whenever a new connection is established.
 */
int             do_daemon = 1;
int             do_fork = 1;
int             do_cleanup = 1;
int             run_silent = 1;

void
usage()
{
	fprintf(stdout, "adiskd [-d] [-n] [-s] \n");
	fprintf(stdout, "\t -d do not run adisk as a daemon \n");
	fprintf(stdout, "\t -n do not fork for a  new connection \n");
	fprintf(stdout, "\t -c do not cleanup *.so files from /tmp \n");
	fprintf(stdout, "\t -s run silently \n");
	fprintf(stdout, "\t -h get this help message \n");
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
		c = getopt(argc, argv, "cdhns");
		if (c == -1) {
			break;
		}
		switch (c) {

			case 'c':
				do_cleanup = 0;
				break;

			case 'd':
				do_daemon = 0;
				break;

			case 'h':
				usage();
				exit(0);
				break;

			case 'n':
				do_fork = 0;
				do_daemon = 0;
				break;

			case 's':
				run_silent = 0;
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
		err = daemon(1, run_silent);
		if (err != 0) {
			perror("daemon call failed !! \n");
			exit(1);
		}
	}


	cb_args.new_conn_cb = search_new_conn;
	cb_args.close_conn_cb = search_close_conn;
	cb_args.start_cb = search_start;
	cb_args.stop_cb = search_stop;
	cb_args.set_searchlet_cb = search_set_searchlet;
	cb_args.set_list_cb = search_set_list;
	cb_args.terminate_cb = search_term;
	cb_args.get_stats_cb = search_get_stats;
	cb_args.release_obj_cb = search_release_obj;
	cb_args.get_char_cb = search_get_char;
	cb_args.get_stats_cb = search_get_stats;
	cb_args.log_done_cb = search_log_done;
	cb_args.setlog_cb = search_setlog;
	cb_args.rleaf_cb = search_read_leaf;
	cb_args.wleaf_cb = search_write_leaf;
	cb_args.lnode_cb = search_list_nodes;
	cb_args.lleaf_cb = search_list_leafs;
	cb_args.sgid_cb = search_set_gid;
	cb_args.clear_gids_cb = search_clear_gids;
	cb_args.set_blob_cb = search_set_blob;
	cb_args.set_offload_cb = search_set_offload;

	cookie = sstub_init(&cb_args);
	if (cookie == NULL) {
		fprintf(stderr, "Unable to initialize the communications\n");
		exit(1);
	}


	/*
	 * We provide the main thread for the listener.  This
	 * should never return.
	 */
	sstub_listen(cookie, do_fork);

	exit(0);
}
