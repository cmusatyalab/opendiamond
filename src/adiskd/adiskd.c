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
#include "lib_odisk.h"
#include "lib_searchlet.h"
#include "lib_sstub.h"
#include "ring.h"
#include "search_state.h"

char *	data_dir = "/opt/dir1/";

int	do_daemon = 1;

int
main(int argc , char **argv) 
{
	int			err;
	void *			cookie;
	sstub_cb_args_t		cb_args;
	int			c;

	/* XXX parse arguments for logging, root directory, ... */

	while (1) {
		c = getopt(argc, argv, "dp:");
		if (c == -1) {
			break;
		}
		switch (c) {
		case 'p':
			data_dir = optarg;
			break;

		case 'd':
			do_daemon = 0;
			break;

		default:
			printf("unknown option %c \n", c);
			exit(1);
		}
			
	}

	printf("data dir <%s> \n", data_dir);	
	

	/* make this a daemon by the appropriate call */
	if (do_daemon) {
		err = daemon(0, 1);
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

	cookie = sstub_init(&cb_args);
	if (cookie == NULL) {
		/* XXX */
		printf("failed to initialize the stub library \n");
		exit(1);
	}


	/*
	 * We provide the main thread for the listener.  This
	 * should never return.
	 */
	sstub_listen(cookie);
	
	exit(0);
}

