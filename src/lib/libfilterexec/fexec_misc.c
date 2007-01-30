/*
 *      Diamond
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
#include <dlfcn.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>
#include <assert.h>
#include <stdint.h>

#include "diamond_consts.h"
#include "diamond_types.h"
#include "lib_tools.h"
#include "lib_odisk.h"
#include "lib_log.h"
#include "sys_attr.h"
#include "lib_filterexec.h"
#include "filter_priv.h"


static char const cvsid[] =
    "$Header$";

int
fexec_num_filters(struct filter_data *fdata)
{


	if (fdata == NULL) {
		return (0);
	} else {
		return (fdata->fd_num_filters);
	}
}



int
fexec_set_blob(filter_data_t * fdata, char *filter_name,
	       int blob_len, void *blob_data)
{
	void           *new_data;
	int             i;

	if (fdata == NULL) {
		return (0);
	}


	for (i = 0; i < fdata->fd_num_filters; i++) {
		if (strcmp(filter_name, fdata->fd_filters[i].fi_name) == 0) {
			if (fdata->fd_filters[i].fi_blob_data != NULL) {
				free(fdata->fd_filters[i].fi_blob_data);
			}

			new_data = malloc(blob_len);
			assert(new_data != NULL);


			memcpy(new_data, blob_data, blob_len);

			fdata->fd_filters[i].fi_blob_len = blob_len;
			fdata->fd_filters[i].fi_blob_data = new_data;
			return (0);
		}
	}
	/*
	 * XXX log 
	 */
	return (ENOENT);
}

void
fexec_set_full_eval(filter_data_t * fdata)
{
	fdata->full_eval = 1;

}

