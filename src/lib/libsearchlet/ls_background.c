/*
 * These file handles a lot of the device specific code.  For the current
 * version we have state for each of the devices.
 */
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/time.h>
#include <assert.h>
#include <stdint.h>
#include <dirent.h>

#include "ring.h"
#include "lib_searchlet.h"
#include "lib_odisk.h"
#include "lib_dctl.h"
#include "dctl_common.h"
#include "lib_search_priv.h"
#include "filter_exec.h"

/* XXX put later */
#define	BG_RING_SIZE	512


#define	BG_STOPPED	0x01
#define	BG_STARTED	0x01

typedef enum {
	BG_STOP,
	BG_START,
	BG_SEARCHLET,
} bg_op_type_t;

typedef struct {
	bg_op_type_t	cmd;
	bg_op_type_t	ver_id;
	char *		filter_name;
	char *		spec_name;
} bg_cmd_data_t;

/*
 * The main loop that the background thread runs to process
 * the data coming from the individual devices.
 */

static void *
bg_main(void *arg)
{
	obj_data_t *		new_obj;
	obj_info_t *		obj_info;
	search_context_t *	sc;
	int			err;
	bg_cmd_data_t *		cmd;
	int			any;
	device_handle_t *	cur_dev;
	struct timespec timeout;
	uint32_t			loop_count = 0;
	uint32_t			dummy = 0;

	err = dctl_register_node(HOST_PATH, HOST_BACKGROUND);
	assert(err == 0);
	err = dctl_register_leaf(HOST_BACKGROUND_PATH, "loop_count", DCTL_DT_UINT32,
					dctl_read_uint32, dctl_write_uint32, &loop_count);
	assert(err == 0);

	err = dctl_register_leaf(HOST_BACKGROUND_PATH, "dummy", DCTL_DT_UINT32,
					dctl_read_uint32, dctl_write_uint32, &dummy);
	assert(err == 0);

	sc = (search_context_t *)arg;

	/*
	 * There are two main tasks that this thread does. The first
	 * is to look for commands from the main process that tell
	 * what to do.  The commands are stored in the "bg_ops" ring.
	 * Typical commands are to load searchlet, stop search, etc.
	 *
	 * The second task is to objects that have arrived from
	 * the storage devices and finish processing them. The
	 * incoming objects are in the "unproc_ring".  After
	 * processing they are released or placed into the "proc_ring"
	 * ring bsed on the results of the evaluation.
	 */
	
	while (1) {
		loop_count++;

		/*
		 * This code processes the objects that have not yet
		 * been fully processed.
		 */
		if (sc->bg_status & BG_STARTED) { 
			obj_info = (obj_info_t *)ring_deq(sc->unproc_ring);
			if (obj_info != NULL) {
				new_obj = obj_info->obj;
				/* printf("bacground:  new obj %p \n", new_obj); */
				/* 
			 	 * Make sure the version number is the
				 * latest.  If it is not equal, then this
				 * is probably data left over from a previous
				 * search that is working its way through
				 * the system.
				 */
				if (sc->cur_search_id != obj_info->ver_num) {
					/* printf(" object bad ver %d %d\n",
							sc->cur_search_id,
							obj_info->ver_num);
							*/
					ls_release_object(sc, new_obj); 
					free(obj_info);
					continue;
				}
				free(obj_info);

				/*
				 * Now that we have an object, go ahead
				 * an evaluated all the filters on the
				 * object.
				 */
				err = eval_filters(new_obj, sc->bg_fdata);
				if (err == 0) {
					/* XXX printf("releasing object \n");*/
					ls_release_object(sc, new_obj);
				} else {
					/* XXXprintf("putting object on ring \n"); */
					err = ring_enq(sc->proc_ring, (void *)new_obj);
					if (err) {
						/* XXX handle overflow gracefully !!! */
						/* XXX log */
					}
				}
					

			} else {
				/*
				 * These are no objects.  See if all the devices
				 * are done.
				 */

				any = 0;
				cur_dev = sc->dev_list;
				while (cur_dev != NULL) {
					if ((cur_dev->flags & DEV_FLAG_COMPLETE) == 0){
						any = 1;
						break;
					}
					cur_dev = cur_dev->next;
				}

				if (any == 0) {
					sc->cur_status = SS_DONE;
				} 
			}
	
		} else {
			/*
			 * There are no objects.  See if all devices
			 * are done.
			 */

		}
	

		/*
		 * This section looks for any commands on the bg ops 
		 * rings and processes them.
		 */ 
		cmd = (bg_cmd_data_t *) ring_deq(sc->bg_ops);
		if (cmd != NULL) {
			switch(cmd->cmd) {
				case BG_SEARCHLET:
					sc->bg_status |= BG_STARTED;
					err = init_filters(cmd->filter_name,
						     cmd->spec_name, 
						     &sc->bg_fdata);
					assert(!err);
					break;

				default:
					printf("background !! \n");
					break;

			}
		}



		timeout.tv_sec = 0;
		timeout.tv_nsec = 10000000; /* 10 ms */
		nanosleep(&timeout, NULL);


	}
}


/*
 * This function sets the searchlet for the background thread to use
 * for processing the data.  This function doesn't actually load
 * the searchlet, puts a command into the queue for the background
 * thread to process that points to the new searchlet.  
 *
 * Having a single thread processing the real operations avoids
 * any ordering/deadlock/etc. problems that may arise. 
 *
 * XXX TODO:  we need to copy the thread into local memory.
 * 	      we need to test the searchlet so we can fail synchronously. 
 */

int
bg_set_searchlet(search_context_t *sc, int id, char *filter_name,
	         char *spec_name)
{
	bg_cmd_data_t *		cmd;

	/*
	 * Allocate a command struct to store the new command.
	 */
	cmd = (bg_cmd_data_t *)malloc(sizeof(*cmd));
	if (cmd == NULL) {
		/* XXX log */
		/* XXX error ? */
		return (0);
	}

	cmd->cmd = BG_SEARCHLET;
	/* XXX local storage for these !!! */
	cmd->filter_name = filter_name;
	cmd->ver_id = id;
	cmd->spec_name = spec_name;

	ring_enq(sc->bg_ops, (void *)cmd);
	return(0);
}


/*
 *  This function intializes the background processing thread that
 *  is used for taking data ariving from the storage devices
 *  and completing the processing.  This thread initializes the ring
 *  that takes incoming data.
 */

int
bg_init(search_context_t *sc, int id)
{
	int		err;
	pthread_t	thread_id;		

	/*
	 * Initialize the ring of commands for the thread.
	 */
	err = ring_init(&sc->bg_ops, BG_RING_SIZE);
	if (err) {
		/* XXX err log */
		return(err);
	}

	/*
	 * Create a thread to handle background processing.
	 */
	err = pthread_create(&thread_id, NULL, bg_main, (void *)sc);
	if (err) {
		/* XXX log */
		printf("failed to create background thread \n");
		return(ENOENT);
	}
	return(0);
}

