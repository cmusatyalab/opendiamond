
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
