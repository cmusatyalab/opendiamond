#include <pthread.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <dirent.h>
#include "obj_attr.h"
#include "lib_od.h"
#include "lib_odisk.h"
#include "lib_searchlet.h"
#include "lib_dctl.h"
#include "lib_sstub.h"
#include "filter_exec.h"
#include "ring.h"
#include "search_state.h"

/* the default directory that holds our data */
char *	data_dir = "/opt/dir1/";



/*
 * The default behaviors are to create a new daemon at startup time
 * and to fork whenever a new connection is established.
 */
int	do_daemon = 1;
int	do_fork = 1;

void
usage()
{
	fprintf(stdout, "adiskd [-d] [-n] [-p <pathname>] \n");
	fprintf(stdout, "\t -d do not run adisk as a daemon \n");
	fprintf(stdout, "\t -n do not fork for a  new connection \n");
	fprintf(stdout, "\t -p <pathname>  set alternative data directory \n");
	fprintf(stdout, "\t -h get this help message \n");
}


int
main(int argc , char **argv) 
{
	int			err;
	void *			cookie;
	sstub_cb_args_t		cb_args;
	int			c;

	/*
	 * Parse any of the command line arguments.
	 */

	while (1) {
		c = getopt(argc, argv, "dhnp:");
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

		case 'n':
			do_fork = 0;
			do_daemon = 0;
			break;

		case 'p':
			data_dir = optarg;
			break;

		default:
			usage();
			exit(1);
			break;
		}
			
	}

	

	/* make this a daemon by the appropriate call */
	if (do_daemon) {
		err = daemon(1, 1);
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
	cb_args.set_list_cb =  search_set_list;
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

	cookie = sstub_init(&cb_args);
	if (cookie == NULL) {
		/* XXX */
		fprintf(stderr, 
			"Unable to initialize the communications library\n");
		exit(1);
	}


	/*
	 * We provide the main thread for the listener.  This
	 * should never return.
	 */
	sstub_listen(cookie, do_fork);
	
	exit(0);
}

