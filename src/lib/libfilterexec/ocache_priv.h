/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 3
 *
 *  Copyright (c) 2002-2005 Intel Corporation
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#ifndef	_OCACHE_PRIV_H_
#define	_OCACHE_PRIV_H_ 	1

#define	MAX_DIR_PATH	512

#include "lib_filter.h"
#include "lib_filterexec.h"

void ceval_wattr_stats(off_t len);


int digest_cal(filter_data_t *fdata, char *filter_name, char *function_name,
	       int numarg, char **filt_args, int blob_len, void *blob,
	       sig_val_t * signature);

void ocache_add_initial_attrs(lf_obj_handle_t ohandle);
int cache_reset_current_attrs(query_info_t *qid, sig_val_t *idsig);

int cache_lookup(sig_val_t *id_sig, sig_val_t *fsig, query_info_t *qid,
		 int *err, int64_t *cache_entry);

void cache_combine_attr_set(query_info_t *qid, int64_t cache_entry);
int cache_read_oattrs(obj_attr_t *attr, int64_t cache_entry);

int ocache_stop_search(sig_val_t *fsig);

int ocache_add_start(lf_obj_handle_t ohandle, sig_val_t *fsig);
int ocache_add_end(lf_obj_handle_t ohandle, sig_val_t *fsig, int conf,
		   query_info_t *qid, filter_exec_mode_t exec_mode,
		   struct timespec *elapsed);



#endif	/* !_OCACHE_PRIV_H_ */

