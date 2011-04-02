/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 4
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

#ifndef _LIB_FILTEREXEC_H_
#define _LIB_FILTEREXEC_H_ 1


#include "lib_odisk.h"

#ifdef __cplusplus
extern          "C"
{
#endif

struct filter_data;
typedef struct filter_data filter_data_t;

/* XXX */
#define	MAX_OBJ_FILES	64

typedef struct {
	sig_val_t	spec_sig;
	int		num_objfiles;
	sig_val_t	obj_sigs[MAX_OBJ_FILES];
} filter_config_t;


/*
 * functions
 */

void fexec_system_init(void);

int  fexec_load_spec(filter_data_t ** fdata, sig_val_t *sig);

int  fexec_load_obj(filter_data_t * fdata, sig_val_t *sig);

int             fexec_init_search(filter_data_t * fdata);

int             fexec_term_search(filter_data_t * fdata);

int             fexec_num_filters(filter_data_t * fdata);

double          fexec_get_load(filter_data_t * fdata);

int             fexec_set_blob(filter_data_t * fdata, char *filter_name,
			       int blob_len, void *blob_data);

int             fexec_get_stats(filter_data_t * fdata, int max,
				filter_stats_t * fstats);

float           fexec_get_prate(filter_data_t *fdata);

void            fexec_set_full_eval(filter_data_t * fdata);

#ifdef __cplusplus
}
#endif
#endif                          /* ! _LIB_FILTEREXEC_H_ */
