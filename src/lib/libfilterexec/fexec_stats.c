
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>
#include <assert.h>
#include <stdint.h>

#include "lib_odisk.h"
#include "lib_log.h"
#include "lib_searchlet.h"
#include "attr.h"
#include "filter_priv.h"


int
fexec_num_filters(struct filter_info *finfo)
{
	filter_info_t *	cur_filt;
	int		count = 0;


	cur_filt = finfo;

	while (cur_filt != NULL) {
		count++;	
		cur_filt = cur_filt->fi_next;
	}

	return(count);
}
