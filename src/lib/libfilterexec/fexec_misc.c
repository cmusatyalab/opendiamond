
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


    if (fdata == NULL) {
        return(0);
    } else { 
        return(fdata->fd_num_filters);
    }
}



int     
fexec_set_blob(filter_data_t *fdata, char *filter_name, 
				int blob_len, void *blob_data)
{
	void *	new_data;
	int		i;

    if (fdata == NULL) {
        return(0);
    }


	for (i=0; i < fdata->fd_num_filters; i++) {
		if (strcmp(filter_name, fdata->fd_filters[i].fi_name) == 0) {
			if (fdata->fd_filters[i].fi_blob_data != NULL) {
				free(fdata->fd_filters[i].fi_blob_data);	
			}

			new_data = malloc(blob_len);
			assert(new_data != NULL);


			memcpy(new_data, blob_data, blob_len);

			fdata->fd_filters[i].fi_blob_len = blob_len;
			fdata->fd_filters[i].fi_blob_data = new_data;
			return(0);
		}
	}
	/* XXX log */
	return(ENOENT);
}

