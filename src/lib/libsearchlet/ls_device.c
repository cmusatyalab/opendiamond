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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>
#include "ring.h"
#include "rstat.h"
#include "lib_searchlet.h"
#include "lib_odisk.h"
#include "lib_search_priv.h"
#include "filter_priv.h"	/* to read stats -RW */
#include "consts.h"
#include "log.h"
#include "log_impl.h"
#include "assert.h"
#include "lib_hstub.h"


/*
 *  This function intializes the background processing thread that
 *  is used for taking data ariving from the storage devices
 *  and completing the processing.  This thread initializes the ring
 *  that takes incoming data.
 */

void
dev_log_data_cb(void *cookie, char *data, int len, int devid)
{

	log_info_t *		linfo;
	device_handle_t *	dev;


	dev = (device_handle_t *)cookie;

	linfo = (log_info_t *)malloc(sizeof(*linfo));
	if (linfo == NULL) {
		/* we failed to allocate the space, we just
		 * free the log data.
		 */
		free(data);
		return;
	}



	linfo->data = data;
	linfo->len = len;
	linfo->dev = devid;

	if (ring_enq(dev->sc->log_ring, (void *)linfo)) {
		/*
		 * If we failed to log, then we just fee
		 * the information and loose this log.
		 */
		free(data);
		free(linfo);
		return;	
	}

	return;
}


/* XXX  ret type??*/
int
dev_new_obj_cb(void *hcookie, obj_data_t *odata, int ver_no)
{

	device_handle_t *	dev;
	int			err;
	obj_info_t *		oinfo;
	dev = (device_handle_t *)hcookie;

	oinfo = (obj_info_t *)malloc (sizeof(*oinfo));
	if (oinfo == NULL ) {
		printf("XXX failed oinfo malloc \n");
		exit(1);
	}
	oinfo->ver_num = ver_no; /* XXX XXX */
	oinfo->obj = odata;	
	err = ring_enq(dev->sc->unproc_ring, (void *)oinfo);
	if (err) {
			/* XXX */
		printf("ring_enq failed \n");
	}
	return(0);
}

void
dev_search_done_cb(void *hcookie, int ver_no)
{

	device_handle_t *	dev;
	dev = (device_handle_t *)hcookie;

	if (dev->sc->cur_search_id != ver_no) {
		/* XXX */
		printf("search done but vno doesn't match !!! \n");
		return;
	}

	printf("search done \n");
	dev->flags |= DEV_FLAG_COMPLETE;

	return;
}




static device_handle_t *
lookup_dev_by_id(search_context_t *sc, uint32_t devid)
{
	device_handle_t *cur_dev;

	cur_dev = sc->dev_list;
	while(cur_dev != NULL) {
		if (cur_dev->dev_id == devid) {
			break;
		}
		cur_dev = cur_dev->next;
	}

	return(cur_dev);	
}


static device_handle_t *
create_new_device(search_context_t *sc, uint32_t devid)
{
	device_handle_t *new_dev;
	hstub_cb_args_t	cb_data;

	new_dev = (device_handle_t *)malloc(sizeof(*new_dev));
	if (new_dev == NULL) {
		/* XXX log */
		printf("XXX to create new device \n"); 
		return(NULL);
	}

	new_dev->flags = 0;
	new_dev->sc = sc;
	new_dev->dev_id = devid;
	new_dev->num_groups = 0;

	cb_data.new_obj_cb = dev_new_obj_cb;
	cb_data.log_data_cb  = dev_log_data_cb;
	cb_data.search_done_cb  = dev_search_done_cb;


	new_dev->dev_handle = device_init(sc->cur_search_id, devid, 
			(void *)new_dev, &cb_data);
	if (new_dev->dev_handle == NULL) {
		/* XXX log */
		printf("device init failed \n");
		free(new_dev);
		return (NULL);
	}

	/*
	 * Put this device on the list of devices involved
	 * in the search.
	 */
	new_dev->next = sc->dev_list;
	sc->dev_list = new_dev;

	/* XXX log */

	return(new_dev);
}


int
lookup_group_hosts(group_id_t gid, int *num_hosts, uint32_t *hostids)
{
	static gid_map_t *	gid_map = NULL;
	gid_map_t *		cur_map;
	int			i;


	if (gid_map == NULL) {
		/* XXX */
		gid_map = read_gid_map("gid_map");
	}

	if (gid_map == NULL) {
		*num_hosts = 0;
		return(ENOENT);
	}


	cur_map = gid_map;

	while (cur_map != NULL) {
		if (cur_map->gid == gid) {
			break;
		}

		cur_map = cur_map->next;
	}


	if (cur_map == NULL) {
		*num_hosts = 0;
		return(ENOENT);
	}


	if (cur_map->num_dev > *num_hosts) {
		/* XXX log */
		*num_hosts = cur_map->num_dev;
		return(ENOMEM);
	}

	for (i = 0; i < cur_map->num_dev; i++) {
		hostids[i] = cur_map->devs[i];
	}
	*num_hosts = cur_map->num_dev;
	return(0);
}

int
device_add_gid(search_context_t *sc, group_id_t gid, uint32_t devid)
{

	device_handle_t * 	cur_dev;

	cur_dev = lookup_dev_by_id(sc, devid);
	if (cur_dev == NULL) {
		cur_dev = create_new_device(sc, devid);
		if (cur_dev == NULL) {
			/* XXX log */
			return(ENOENT);
		}
	}
	/*
	 * check to see if we can add more groups, if so add it to the list
	 */
	if (cur_dev->num_groups >= MAX_DEV_GROUPS) {
		/* XXX log */
		return(ENOENT);
	}

	cur_dev->dev_groups[cur_dev->num_groups] = gid;
	cur_dev->num_groups++;
	return(0);
}


