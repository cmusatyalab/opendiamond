/*
 *
 *
 *                          Diamond 1.0
 * 
 *            Copyright (c) 2002-2004, Intel Corporation
 *                         All Rights Reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 *    * Neither the name of Intel nor the names of its contributors may
 *      be used to endorse or promote products derived from this software 
 *      without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
#include "lib_od.h"
#include "obj_attr.h"
#include "lib_odisk.h"
#include "odisk_priv.h"


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





