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
#include <string.h>
#include <stdint.h>
#include <dirent.h>

#include "ring.h"
#include "lib_searchlet.h"
#include "lib_od.h"
#include "lib_odisk.h"
#include "lib_dctl.h"
#include "lib_log.h"
#include "lib_hstub.h"
#include "dctl_common.h"
#include "rstat.h"
#include "lib_search_priv.h"
#include "filter_exec.h"

/* XXX put later */
#define	BG_RING_SIZE	512


#define	BG_STARTED	0x01
#define	BG_SET_SEARCHLET	0x02

/* XXX debug for now, enables cpu based load splitting */
uint32_t	do_cpu_update	=  0;

typedef enum {
	BG_STOP,
	BG_START,
	BG_SEARCHLET,
	BG_SET_BLOB,
} bg_op_type_t;

/* XXX huge hack */
typedef struct {
	bg_op_type_t	cmd;
	bg_op_type_t	ver_id;
	char *			filter_name;
	char *			spec_name;
	void *			blob;
	int				blob_len;
} bg_cmd_data_t;



void
update_rates(search_context_t *sc) 
{
	double	load;
	device_handle_t	*	cur_dev;
	int			dev_cnt = 0;
	uint64_t		val;
	uint64_t		target;
	int			err;

	if (do_cpu_update == 0) {
		return;
	}
	load = fexec_get_load(sc->bg_fdata);

	r_cpu_freq(&val);

	for (cur_dev = sc->dev_list; cur_dev != NULL; cur_dev = cur_dev->next) {		dev_cnt++;
	}

	target = (uint64_t)((double)val * load/(double)dev_cnt);

	for (cur_dev = sc->dev_list; cur_dev != NULL; cur_dev = cur_dev->next) {		dev_cnt++;
		err = device_set_offload(cur_dev->dev_handle, 
			sc->cur_search_id, target);
		assert(err == 0);
	}
}
		

static void
refill_credits(search_context_t *sc)
{
	device_handle_t *	cur_dev;

	for (cur_dev = sc->dev_list; cur_dev != NULL; cur_dev = cur_dev->next) {
		cur_dev->cur_credits += cur_dev->credit_incr;
		if (cur_dev->credit_incr > MAX_CUR_CREDIT) {
				cur_dev->credit_incr = MAX_CUR_CREDIT;
		}
	}
}

/* XXX constant config */
#define         POLL_SECS       1
#define         POLL_USECS      0

obj_info_t *
get_next_object(search_context_t *sc)
{
	device_handle_t *	cur_dev;
	obj_info_t	*		obj_inf;
	int	 				loop = 0;


	if (sc->last_dev == NULL) {
		cur_dev = sc->dev_list;
	} else {
		cur_dev = sc->last_dev->next;
	}

redo:
	while (cur_dev != NULL) {
		if (cur_dev->cur_credits > 0) {
			obj_inf = device_next_obj(cur_dev->dev_handle);
			if (obj_inf != NULL) {
				cur_dev->cur_credits--;
				sc->last_dev = cur_dev;
				return(obj_inf);
			}
		}
		cur_dev = cur_dev->next;
	}

	/* if we fall through and it is our first iteration
	 * then retry from the beggining.
	 */ 
	if (loop == 0) {
		loop = 1;
		cur_dev = sc->dev_list;
		goto redo;
	} else if (loop == 1) {
		loop = 2;
		cur_dev = sc->dev_list;
		refill_credits(sc);
		goto redo;
	}

	return(NULL);
}


int
bg_val(void *cookie)
{
	return(1);
}

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
	int					err;
	bg_cmd_data_t *		cmd;
	int					any;
	device_handle_t *	cur_dev;
	struct timeval		this_time;
	struct timeval		next_time = {0,0};
	struct timezone		tz;
	struct timespec 	timeout;
	uint32_t			loop_count = 0;
	uint32_t			dummy = 0;

	sc = (search_context_t *)arg;

	dctl_thread_register(sc->dctl_cookie);
	log_thread_register(sc->log_cookie);

	err = dctl_register_node(HOST_PATH, HOST_BACKGROUND);
	assert(err == 0);
	err = dctl_register_leaf(HOST_BACKGROUND_PATH, "loop_count", DCTL_DT_UINT32,
					dctl_read_uint32, dctl_write_uint32, &loop_count);
	assert(err == 0);

	err = dctl_register_leaf(HOST_BACKGROUND_PATH, "cpu_split", 
			DCTL_DT_UINT32, dctl_read_uint32, 
			dctl_write_uint32, &do_cpu_update);
	assert(err == 0);


	err = dctl_register_leaf(HOST_BACKGROUND_PATH, "dummy", DCTL_DT_UINT32,
					dctl_read_uint32, dctl_write_uint32, &dummy);
	assert(err == 0);

	err = dctl_register_leaf(HOST_BACKGROUND_PATH, "credit_policy", 
					DCTL_DT_UINT32, dctl_read_uint32, dctl_write_uint32, 
					&sc->bg_credit_policy);
	assert(err == 0);


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
		if ((sc->bg_status & BG_STARTED)  &&
			(ring_count(sc->proc_ring) < sc->pend_lw)) {
			obj_info = get_next_object(sc);
			if (obj_info != NULL) {
				new_obj = obj_info->obj;
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

				/*
				 * Now that we have an object, go ahead
				 * an evaluated all the filters on the
				 * object.
				 */
				err = eval_filters(new_obj, sc->bg_fdata, 1, sc, bg_val, NULL);
				if (err == 0) {
					ls_release_object(sc, new_obj);
					free(obj_info);
				} else {
					err = ring_enq(sc->proc_ring, (void *)obj_info);
					if (err) {
						/* XXX handle overflow gracefully !!! */
						/* XXX log */
						assert(0);
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

				if ((any == 0) && (sc->cur_status == SS_ACTIVE)) {
					sc->cur_status = SS_DONE;
				} 
			}
	
		} else {
			/*
			 * There are no objects.  See if all devices
			 * are done.
			 */
			

		}


	
		/* timeout look that runs once a second */
		gettimeofday(&this_time, &tz);

		if (((this_time.tv_sec == next_time.tv_sec) &&
		    	(this_time.tv_usec >= next_time.tv_usec)) ||
				(this_time.tv_sec > next_time.tv_sec)) {
		
			update_rates(sc);

			assert(POLL_USECS < 1000000);
			next_time.tv_sec = this_time.tv_sec + POLL_SECS;
			next_time.tv_usec = this_time.tv_usec + POLL_USECS;

			if (next_time.tv_usec >= 1000000) {
				next_time.tv_usec -= 1000000;
				next_time.tv_sec += 1;
			}
		}


		/*
		 * This section looks for any commands on the bg ops 
		 * rings and processes them.
		 */ 
		cmd = (bg_cmd_data_t *) ring_deq(sc->bg_ops);
		if (cmd != NULL) {
			switch(cmd->cmd) {
				case BG_SEARCHLET:
					sc->bg_status |= BG_SET_SEARCHLET;
					err = fexec_load_searchlet(cmd->filter_name,
						     cmd->spec_name, 
						     &sc->bg_fdata);
					assert(!err);
					break;

				case BG_SET_BLOB:
					fexec_set_blob(sc->bg_fdata, cmd->filter_name,
							cmd->blob_len, cmd->blob);	
					assert(!err);
					break;

				case BG_START:
					/* XXX reinit filter is not new one */
					if (!(sc->bg_status & BG_SET_SEARCHLET)) {
						printf("start: no searchlet\n");
						break;
					}
					/* XXX clear out the proc ring */
					{
						obj_data_t *		new_obj;
						obj_info_t *		obj_info;

						while(!ring_empty(sc->proc_ring)) {
							/* XXX lock */
							obj_info = (obj_info_t *)ring_deq(sc->proc_ring);
							new_obj = obj_info->obj;
							ls_release_object(sc, new_obj); 
							free(obj_info);
						}
					}
			
					/* XXX clean up any stats ?? */

					fexec_init_search(sc->bg_fdata);	
					sc->bg_status |= BG_STARTED;
					break;
					
				case BG_STOP:
					sc->bg_status &= ~BG_STARTED;
					/* XXX toher state ?? */
					fexec_term_search(sc->bg_fdata);
					break;

				default:
					printf("background !! \n");
					break;

			}
			free(cmd);
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
	cmd->ver_id = (bg_op_type_t)id;
	cmd->spec_name = spec_name;

	ring_enq(sc->bg_ops, (void *)cmd);
	return(0);
}


int
bg_start_search(search_context_t *sc, int id)
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

	cmd->cmd = BG_START;
	ring_enq(sc->bg_ops, (void *)cmd);
	return(0);
}

int
bg_stop_search(search_context_t *sc, int id)
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

	cmd->cmd = BG_START;
	ring_enq(sc->bg_ops, (void *)cmd);
	return(0);
}

int
bg_set_blob(search_context_t *sc, int id, char *filter_name,
				int blob_len, void *blob_data)
{
	bg_cmd_data_t *		cmd;
	void *				new_blob;

	/*
	 * Allocate a command struct to store the new command.
	 */
	cmd = (bg_cmd_data_t *)malloc(sizeof(*cmd));
	if (cmd == NULL) {
		/* XXX log */
		/* XXX error ? */
		return (0);
	}

	new_blob = malloc(blob_len);
	assert(new_blob != NULL);
	memcpy(new_blob, blob_data, blob_len);


	cmd->cmd = BG_SET_BLOB;
	/* XXX local storage for these !!! */
	cmd->filter_name = strdup(filter_name);
	assert(cmd->filter_name != NULL);

	cmd->blob_len = blob_len;
	cmd->blob = new_blob;

	
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
	err = pthread_create(&thread_id, PATTR_DEFAULT, bg_main, (void *)sc);
	if (err) {
		/* XXX log */
		printf("failed to create background thread \n");
		return(ENOENT);
	}
	return(0);
}

