#include <pthread.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <unistd.h>
#include <errno.h>
#include "obj_attr.h"
#include "lib_odisk.h"
#include "lib_searchlet.h"
#include "lib_sstub.h"
#include "ring.h"
#include "search_state.h"


int
main(int argc , char **argv) 
{
	int			err;
	void *			cookie;
	sstub_cb_args_t		cb_args;

	/* XXX parse arguments for logging, root directory, ... */


	/* make this a daemon by the appropriate call */

#ifdef	XXX
	err = daemon(0, 1);
	if (err != 0) {
		perror("daemon call failed !! \n");
		exit(1);
	}

#endif

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

