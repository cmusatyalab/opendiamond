
/* optimizers for filter execution. */

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "fexec_opt.h"
typedef struct filter_data filter_data_t;
#include "fexec_stats.h"

extern void update_filter_order(filter_data_t *fdata, const permutation_t *perm);


/* ********************************************************************** */


void *
hill_climb_new(const filter_data_t *fdata) {
  hc_state_t *hc = (hc_state_t *)malloc(sizeof(hc_state_t));
  char buf[BUFSIZ];

  if(hc) {
    hill_climb_init(hc, fdata->fd_perm);
  }

  printf("hill climb starts at: %s\n", pmPrint(fdata->fd_perm, buf, BUFSIZ));
  return (void *)hc;
}

void
hill_climb_delete(void *context) {
	hc_state_t *hc = (hc_state_t *)context;
	hill_climb_cleanup(hc);
	free(hc);
}

void
hill_climb_optimize(void *context, filter_data_t *fdata) {
	int err = 0;
	hc_state_t *hc = (hc_state_t *)context;
	char buf[BUFSIZ];
	static int optimizer_done = 0; /* time before restart */

	printf("hc-opt\n");

	if(optimizer_done > 0) {
		optimizer_done--;
		/* restart hill climbing (in case we have better data now */
		if(!optimizer_done) {
			printf("restarting hill climb\n");
			hill_climb_cleanup(hc);
			hill_climb_init(hc, fdata->fd_perm);
		}
		return;
	}

	while(!err) {
		//printf("hc\n");
		err = hill_climb_step(hc, fdata->fd_po, 
				      (evaluation_func_t)fexec_evaluate, fdata);
	}
	switch(err) {
	case RC_ERR_COMPLETE:
		printf("hill climb optimizer suggests: %s\n", 
		       pmPrint(hill_climb_result(hc), buf, BUFSIZ));
		fexec_print_cost(fdata, hill_climb_result(hc)); printf("\n");
		/* update and restart */
		if(pmEqual(fdata->fd_perm, hill_climb_result(hc))) {
			printf("hill climbing didn't find further improvement\n");
			optimizer_done = 5;
			//hill_climb_cleanup(hc);
		} else {
			update_filter_order(fdata, hill_climb_result(hc));
			hill_climb_cleanup(hc);
			hill_climb_init(hc, fdata->fd_perm);
		}
		break;
	case RC_ERR_NODATA:
		printf("hill climb optimizer needs more data for: %s\n", 
		       pmPrint(hill_climb_next(hc), buf, BUFSIZ));
		update_filter_order(fdata, hill_climb_next(hc));
		break;
	default:
		break;		
	}
		
}

/* ********************************************************************** */

void *
best_first_new(const filter_data_t *fdata) {
  bf_state_t *bf = (bf_state_t *)malloc(sizeof(bf_state_t));
  char buf[BUFSIZ];

  if(bf) {
    best_first_init(bf, pmLength(fdata->fd_perm), fdata->fd_po, 
		    (evaluation_func_t)fexec_evaluate, fdata);
  }

  printf("best_first starts at: %s\n", pmPrint(fdata->fd_perm, buf, BUFSIZ));
  return (void *)bf;
}

void
best_first_delete(void *context) {
	bf_state_t *bf = (bf_state_t *)context;
	best_first_cleanup(bf);
	free(bf);
}

void
best_first_optimize(void *context, filter_data_t *fdata) {
	int err = 0;
	bf_state_t *bf = (bf_state_t *)context;
	char buf[BUFSIZ];
	static int optimizer_done = 0; /* time before restart */

	printf("bf-opt\n");

	if(optimizer_done > 0) {
		optimizer_done--;
		/* restart hill climbing (in case we have better data now */
		if(!optimizer_done) {
			printf("--- restarting optimizer ------------------------------\n");
			best_first_cleanup(bf);
			best_first_init(bf, pmLength(fdata->fd_perm), fdata->fd_po, 
					(evaluation_func_t)fexec_evaluate, fdata);
		}
		return;
	}

	while(!err) {
		//printf("bf\n");
	  err = best_first_step(bf);
	}
	switch(err) {
	case RC_ERR_COMPLETE:
		printf("optimizer done: %s\n", 
		       pmPrint(best_first_result(bf), buf, BUFSIZ));
		fexec_print_cost(fdata, best_first_result(bf)); printf("\n");
		/* update and restart */
		if(pmEqual(fdata->fd_perm, best_first_result(bf))) {
			printf("optimizer didn't find further improvement\n");
			optimizer_done = 10;
			//best_first_cleanup(bf);
		} else {
			update_filter_order(fdata, best_first_result(bf));
			best_first_cleanup(bf);
			best_first_init(bf, pmLength(fdata->fd_perm), fdata->fd_po, 
					(evaluation_func_t)fexec_evaluate, fdata);
		}
		break;
	case RC_ERR_NODATA:
		printf("optimizer needs more data for: %s\n", 
		       pmPrint(best_first_next(bf), buf, BUFSIZ));
		printf("\t"); fexec_print_cost(fdata, best_first_next(bf)); printf("\n");
		update_filter_order(fdata, best_first_next(bf));
		break;
	default:
		break;		
	}
		
}

