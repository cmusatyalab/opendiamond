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
#include "ring.h"
#include "lib_searchlet.h"
#include "obj_attr.h"
#include "lib_search_priv.h"


typedef enum {
	DEV_STOP,
	DEV_TERM,
	DEV_START,
	DEV_SEARCHLET
} dev_op_type_t;


typedef struct {
	char *filter;
	char *spec;
} dev_slet_data_t;


typedef struct {
	dev_op_type_t	cmd;
	int		id;
	union {
		dev_slet_data_t	sdata;
	} extra_data;

} dev_cmd_data_t;
	


int
device_stop(device_state_t *dev, int id)
{
	dev_cmd_data_t *cmd;
	int		err;

	cmd = (dev_cmd_data_t *) malloc(sizeof(*cmd));	
	if (cmd == NULL) {
		return (1);
	}

	cmd->cmd = DEV_STOP;
	cmd->id = id;

	err = ring_enq(dev->device_ops, (void *)cmd);
	if (err) {
		free(cmd);
		return (1);
	}
	return (0);
}


int
device_terminate(device_state_t *dev, int id)
{
	dev_cmd_data_t *cmd;
	int		err;

	cmd = (dev_cmd_data_t *) malloc(sizeof(*cmd));	
	if (cmd == NULL) {
		return (1);
	}

	cmd->cmd = DEV_TERM;
	cmd->id = id;

	err = ring_enq(dev->device_ops, (void *)cmd);
	if (err) {
		free(cmd);
		return (1);
	}
	return (0);
}

int
device_start(device_state_t *dev, int id)
{
	dev_cmd_data_t *cmd;
	int		err;

	cmd = (dev_cmd_data_t *) malloc(sizeof(*cmd));	
	if (cmd == NULL) {
		return (1);
	}

	cmd->cmd = DEV_START;
	cmd->id = id;

	err = ring_enq(dev->device_ops, (void *)cmd);
	if (err) {
		free(cmd);
		return (1);
	}
	return (0);
}

int
device_set_searchlet(device_state_t *dev, int id, char *filter, char *spec)
{
	dev_cmd_data_t *cmd;
	int		err;

	cmd = (dev_cmd_data_t *) malloc(sizeof(*cmd));	
	if (cmd == NULL) {
		return (1);
	}

	cmd->cmd = DEV_SEARCHLET;
	cmd->id = id;
	
	cmd->extra_data.sdata.filter = filter;
	cmd->extra_data.sdata.spec = spec;

	err = ring_enq(dev->device_ops, (void *)cmd);
	if (err) {
		free(cmd);
		return (1);
	}
	return (0);
}

static void
dev_process_cmd(device_state_t *dev, dev_cmd_data_t *cmd)
{

	int	err;

	switch (cmd->cmd) {
		case DEV_STOP:
			break;
	
		case DEV_TERM:
			break;

		case DEV_START:
			/*
	 		 * Start the emulated device for now.
	 	  	 * XXX do this for real later.
	 		 */
			err = dev_emul_init(dev);
			if (err) {
				/* XXX log */
				/* XXX crap !! */
				return;
			}
			dev->flags |= DEV_FLAG_RUNNING;
			break;

		case DEV_SEARCHLET:
			dev->ver_no = cmd->id;
			break;

	}
	/* XXX free cmd */

}

/*
 * The main loop that the per device thread runs while
 * processing data to/from the individual devices
 */

static void *
device_main(void *arg)
{
	device_state_t *dev;
	dev_cmd_data_t *cmd;
	obj_data_t*	new_obj;
	obj_info_t *	obj_info;
	int		err;
	pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t  cond = PTHREAD_COND_INITIALIZER;
	struct timeval now;
	struct timespec timeout;
	struct timezone tz;

	tz.tz_minuteswest = 0;
	tz.tz_dsttime = 0;

	dev = (device_state_t *)arg;

	/*
	 * XXX need to open comm channel with device
	 */

	
	while (1) {
		cmd = (dev_cmd_data_t *)ring_deq(dev->device_ops);
		if (cmd != NULL) {
			dev_process_cmd(dev, cmd);
			free(cmd);
		}

			
		/*
		 * XXX look for data from device to process.
		 */
		if ((dev->flags & DEV_FLAG_RUNNING) == DEV_FLAG_RUNNING) {
			err = dev_emul_next_obj(&new_obj, dev);
			if (err == ENOENT) {
				/*
				 * We have processed all the objects,
				 * clear the running and set the complete
				 * flags.
				 */
				dev->flags &= ~DEV_FLAG_RUNNING;
				dev->flags |= DEV_FLAG_COMPLETE;
			} else if (err) {
				printf("dmain: failed to get obj !! \n");
				/* sleep(1); */
				continue;
			} else {
				obj_info = (obj_info_t *)
					malloc(sizeof(*obj_info));
				if (obj_info == NULL) {
					printf("XXX failed to alloc obj inf\n");
					exit(1);
				}
				obj_info->obj = new_obj;
				obj_info->ver_num = dev->ver_no;
			   	err = ring_enq(dev->sc->unproc_ring, 
						(void *)obj_info);
				if (err) {
					/* XXX handle overflow gracefully !!! */
					/* XXX log */
	
				}
			}

		}

		pthread_mutex_lock(&mut);
		gettimeofday(&now, &tz);
		timeout.tv_sec = now.tv_sec + 1;
		timeout.tv_nsec = now.tv_usec * 1000;

		pthread_cond_timedwait(&cond, &mut, &timeout);
		pthread_mutex_unlock(&mut);
	}
}


device_state_t *
device_init(search_context_t *sc, int id)
{
	device_state_t *new_dev;
	int		err;

	new_dev = (device_state_t *) malloc(sizeof(*new_dev));	
	if (new_dev == NULL) {
		return (NULL);
	}

	/*
	 * initialize the fields int device struct.
	 */
	err = ring_init(&new_dev->device_ops);
	if (err) {
		free(new_dev);
		return (NULL);
	}

	new_dev->flags = 0;
	new_dev->sc = sc;

	/* XXX open channel to the new host */

	/*
	 * Spawn a thread for this device that process data to and
	 * from the device.
	 */

	err = pthread_create(&new_dev->thread_id, NULL, device_main, 
			    (void *)new_dev);
	if (err) {
		/* XXX log */
		free(new_dev);
		return (NULL);
	}


	return(new_dev);
}

