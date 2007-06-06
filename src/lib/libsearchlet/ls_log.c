/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 2
 *
 *  Copyright (c) 2002-2005 Intel Corporation
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

/*
 * These file handles a lot of the device specific code.  For the current
 * version we have state for each of the devices.
 */
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <netdb.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <assert.h>
#include <dirent.h>
#include <stdint.h>

#include "lib_tools.h"
#include "lib_searchlet.h"
#include "lib_log.h"
#include "lib_odisk.h"
#include "lib_search_priv.h"
#include "lib_filterexec.h"
#include "log_socket.h"
#include "log_impl.h"
#include "lib_dctl.h"
#include "lib_dconfig.h"
#include "lib_hstub.h"


static char const cvsid[] =
    "$Header$";


#define	LOG_RING_SIZE	512

/*
 * linux specific flag 
 */
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif


static uint32_t last_level,
                last_src;

void
update_device_log_level(search_context_t * sc)
{
	device_handle_t *cur_dev;
	int             err;
	for (cur_dev = sc->dev_list; cur_dev != NULL; cur_dev = cur_dev->next) {
		if (cur_dev->flags & DEV_FLAG_DOWN) {
			continue;
		}
		err =
		    device_set_log(cur_dev->dev_handle, last_level, last_src);
	}

}


void
set_device_log(log_set_level_t * llevel, search_context_t * sc)
{
	device_handle_t *cur_dev;
	uint32_t        hlevel,
	                hsrc;
	int             err;

	hlevel = ntohl(llevel->log_level);
	hsrc = ntohl(llevel->log_src);

	switch (llevel->log_op) {
	case LOG_SETLEVEL_ALL:
		last_level = llevel->log_level;
		last_src = llevel->log_src;

		for (cur_dev = sc->dev_list; cur_dev != NULL;
		     cur_dev = cur_dev->next) {

			err = device_set_log(cur_dev->dev_handle,
					     llevel->log_level,
					     llevel->log_src);
		}

		log_setlevel(hlevel);
		log_settype(hsrc);
		break;

	case LOG_SETLEVEL_DEVICE:
		cur_dev = sc->dev_list;
		while (cur_dev != NULL) {
			if (cur_dev->dev_id == llevel->dev_id) {
				err = device_set_log(cur_dev->dev_handle,
						     llevel->log_level,
						     llevel->log_src);
			}
			cur_dev = cur_dev->next;
		}
		break;


	case LOG_SETLEVEL_HOST:
		log_setlevel(hlevel);
		log_settype(hsrc);
		break;


	default:
		assert(0);
		break;

	}


	/*
	 * XXX handle single device set options 
	 */


}
