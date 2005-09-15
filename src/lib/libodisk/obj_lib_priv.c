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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <stdint.h>
#include "diamond_consts.h"
#include "diamond_types.h"
#include "sig_calc.h"
#include "obj_attr.h"
#include "lib_odisk.h"
#include "odisk_priv.h"

static char const cvsid[] = "$Header$";

#ifdef	XXX_OLD
int
odisk_create_obj(obj_handle_t  *obj_handle, char *name)
{

	obj_state_t *	new_obj;
	struct stat	stats;
	int		err;


	printf("create_obj: <%s> \n", name);

	if (strlen(name) >= MAX_FNAME) {
		/* XXX log error */
		return (EINVAL);
	}

	new_obj = malloc(sizeof(*new_obj));
	if (new_obj == NULL) {
		/* XXX log error */
		return (ENOMEM);
	}
	strcpy(new_obj->os_name, name);

	/* get the length and save it as part of the data */
	err = stat(new_obj->os_name, &stats);
	if (err != 0) {
		free(new_obj);
		return (ENOENT);
	}
	new_obj->os_len = stats.st_size;

	printf("obj len %ld \n", stats.st_size);

	/* open the file */
	new_obj->os_file  = fopen(name, "rb");
	if (new_obj->os_file == NULL) {
		free(new_obj);
		return (ENOENT);
	}

	*obj_handle = (obj_handle_t)new_obj;

	return(0);
}


/*
 * Free the state associated with an object that we don't need anymore.
 * This will happend when when the filters decides it is no longer interested in
 * the object.
 */

int
odisk_free_obj(obj_handle_t *obj_handle)
{
	obj_state_t * 	ostate = (obj_state_t *)obj_handle;
	obj_map_t *	cur_map;
	obj_map_t *	next_map;
	anc_state_t *	cur_state;
	anc_state_t *	next_state;
	int		err;

	/* XXX verify handle */

	/*
	 * First we need to go through the different mappings and free them.
	 */
	cur_map = ostate->os_omap;
	while (cur_map != NULL) {
		next_map = cur_map->om_next;
		free(cur_map);
		cur_map = next_map;
	}


	/*
	 * Now free the ancillary state the was generated along with the object.
	 *
	 * TODO: In the future when caching is enable we will need to store it.
	 */
	cur_state = ostate->os_adata;
	while (cur_state != NULL) {
		next_state = cur_state->anc_next;
		free(cur_state->anc_data);
		free(cur_state);
		cur_state = next_state;
	}


	/* close the FD */
	err = fclose(ostate->os_file);
	if (err) {
		/* XXX log */
		return (EINVAL);
	}

	free(ostate);

	return(0);
}
#endif





