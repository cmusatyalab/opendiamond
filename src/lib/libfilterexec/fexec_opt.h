
/* optimizers for filter execution. */
#include "lib_od.h"
#include "lib_odisk.h"
#include "lib_log.h"
#include "lib_searchlet.h"
#include "attr.h"

#include "filter_exec.h"
#include "filter_priv.h"

struct filter_data;

/* ********************************************************************** */

void *hill_climb_new(struct filter_data *fdata);
void hill_climb_delete(void *);
int hill_climb_optimize(void *context, struct filter_data *fdata);

void *best_first_new(struct filter_data *fdata);
void best_first_delete(void *);
int best_first_optimize(void *context, struct filter_data *fdata);


void *indep_new(struct filter_data *fdata);
void indep_delete(void *);
int indep_optimize(void *context, struct filter_data *fdata);

void *random_new(struct filter_data *fdata);

void *static_new(struct filter_data *fdata);

/* ********************************************************************** */

