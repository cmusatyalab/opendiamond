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
#include <dlfcn.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>
#include <assert.h>
#include <stdint.h>

#include "lib_od.h"
#include "lib_odisk.h"
#include "lib_log.h"
#include "lib_searchlet.h"
#include "attr.h"
#include "filter_exec.h"
#include "filter_priv.h"


int
fexec_num_filters(struct filter_data *fdata)
{


	if (fdata == NULL)
	{
		return (0);
	} else
	{
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
