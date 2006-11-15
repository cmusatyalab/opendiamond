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
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#ifndef _LIB_FILTEREXEC_H_
#define _LIB_FILTEREXEC_H_ 1


#include "rcomb.h"

#ifdef __cplusplus
extern          "C"
{
#endif

struct filter_data;
typedef struct filter_data filter_data_t;

/*
 * optimizer policy setup
 */
enum policy_type_t {
    NULL_POLICY = 0,
    HILL_CLIMB_POLICY,
    BEST_FIRST_POLICY,
    INDEP_POLICY,
    RANDOM_POLICY,
    STATIC_POLICY
};

typedef struct opt_policy_t {
	enum policy_type_t policy;
	void           *(*p_new) (struct filter_data *);
	void            (*p_delete) (void *context);
	int             (*p_optimize) (void *context, struct filter_data *);
	void           *p_context;
	int             exploit;    /* if we are in exploit mode */
} opt_policy_t;

enum bypass_type_t {
    BP_NONE = 0,
    BP_SIMPLE,
    BP_GREEDY,
    BP_HYBRID
};

enum auto_part_t {
    AUTO_PART_NONE = 0,
    AUTO_PART_BYPASS,
    AUTO_PART_QUEUE
};


extern int             fexec_bypass_type;
extern int             fexec_autopart_type;

struct filter_exec_t {
	enum policy_type_t current_policy;
};



/*
 * update at your own risk! 
 */
extern struct filter_exec_t filter_exec;

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

void fexec_system_init();

int  fexec_load_spec(filter_data_t ** fdata, sig_val_t *sig);
int  fexec_load_obj(filter_data_t * fdata, sig_val_t *sig);

int             fexec_init_search(filter_data_t * fdata);
int             fexec_term_search(filter_data_t * fdata);
void	    	optimize_filter_order(filter_data_t * fdata, 
			opt_policy_t * policy);
double	    	tv_diff(struct timeval *end, struct timeval *start);
int             eval_filters(obj_data_t * obj_handle,
			     filter_data_t * fdata, int force_eval,
			     double *elapsed,
			     void *cookie,
			     int (*continue_cb)(void* vookie),
			     int (*cb_func) (void *cookie, char *name,
					     int *pass,
					     uint64_t *
					     et));
int             fexec_num_filters(filter_data_t * fdata);
void            fexec_clear_stats(filter_data_t * fdata);
double          fexec_get_load(filter_data_t * fdata);
int             fexec_set_blob(filter_data_t * fdata, char *filter_name,
			       int blob_len, void *blob_data);
int             fexec_get_stats(filter_data_t * fdata, int max,
				filter_stats_t * fstats);
char           *fexec_cur_filtname();


int             fexec_update_bypass(filter_data_t * fdata, double ratio);
int             fexec_update_grouping(filter_data_t * fdata, double ratio);
float           fexec_get_prate(filter_data_t *fdata);

int             fexec_estimate_cost(filter_data_t * fdata,
				    permutation_t * perm, int gen, int indep,
				    float *cost);
int             fexec_estimate_cur_cost(filter_data_t * fdata,
					float *cost);
void            fexec_set_full_eval(filter_data_t * fdata);

#ifdef __cplusplus
}
#endif
#endif                          /* ! _LIB_FILTEREXEC_H_ */
