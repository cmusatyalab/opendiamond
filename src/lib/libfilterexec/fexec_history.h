/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 2
 *
 *  Copyright (c) 2002-2006 Intel Corporation
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#ifndef _FEXEC_HISTORY_H_
#define _FEXEC_HISTORY_H_

#include <glib.h>
#include "sig_calc.h"

typedef struct {
	sig_val_t	filter_sig;
	unsigned int	executions;
	unsigned int	search_objects;
	unsigned int	filter_objects;
	unsigned int	drop_objects;
	unsigned int	last_run;
} filter_history_t;

typedef struct {
	guint num_entries;
	filter_history_t *entries;
} filter_history_list_t;

GHashTable *get_filter_history();
filter_history_list_t *get_filter_history_by_frequency(GHashTable *histories);
void write_filter_history(GHashTable *histories);
void update_filter_history(GHashTable *filter_histories, gboolean remove);

#endif /*_FEXEC_HISTORY_H_*/
