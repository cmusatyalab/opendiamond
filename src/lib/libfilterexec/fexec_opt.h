
/* optimizers for filter execution. */

#include "filter_priv.h"

struct filter_data;

/* ********************************************************************** */

void *hill_climb_new(const struct filter_data *fdata);
void hill_climb_delete(void *);
void hill_climb_optimize(void *context, struct filter_data *fdata);

void *best_first_new(const struct filter_data *fdata);
void best_first_delete(void *);
void best_first_optimize(void *context, struct filter_data *fdata);




/* ********************************************************************** */

