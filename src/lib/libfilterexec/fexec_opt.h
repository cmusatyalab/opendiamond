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


/*
 * optimizers for filter execution. 
 */
#include "diamond_consts.h"
#include "diamond_types.h"
#include "lib_odisk.h"
#include "lib_log.h"
#include "sys_attr.h"

#include "lib_filterexec.h"
#include "filter_priv.h"

struct filter_data;

/*
 ********************************************************************** */

void           *hill_climb_new(struct filter_data *fdata);
void            hill_climb_delete(void *);
int             hill_climb_optimize(void *context, struct filter_data *fdata);

void           *best_first_new(struct filter_data *fdata);
void            best_first_delete(void *);
int             best_first_optimize(void *context, struct filter_data *fdata);


void           *indep_new(struct filter_data *fdata);
void            indep_delete(void *);
int             indep_optimize(void *context, struct filter_data *fdata);

void           *random_new(struct filter_data *fdata);

void           *static_new(struct filter_data *fdata);

/*
 ********************************************************************** */
