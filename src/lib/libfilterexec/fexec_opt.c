
/* optimizers for filter execution. */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#include "fexec_opt.h"
#include "fexec_stats.h"

extern void update_filter_order(filter_data_t *fdata, const permutation_t *perm);


// #define VERBOSE 1

#ifdef VERBOSE
#define   IFVERBOSE
#else
#define   IFVERBOSE if(0)
#endif

static const int RESTART_INTERVAL = 200;

/* ********************************************************************** */


void *
hill_climb_new(const filter_data_t *fdata) {
  hc_state_t *hc = (hc_state_t *)malloc(sizeof(hc_state_t));
  char buf[BUFSIZ];

  hc->global_best = NULL;	/* XXX */

  if(hc) {
    hill_climb_init(hc, fdata->fd_perm, fdata->fd_po);
  }
  IFVERBOSE printf("hill climb starts at: %s\n", pmPrint(fdata->fd_perm, buf, BUFSIZ));
  return (void *)hc;
}

void
hill_climb_delete(void *context) {
  hc_state_t *hc = (hc_state_t *)context;

  hill_climb_cleanup(hc);
  if(hc->global_best) {		/* XXX */
    pmDelete(hc->global_best);
  }
  
  free(hc);
}

/* return true if we are currently exploiting a permutation */
int
hill_climb_optimize(void *context, filter_data_t *fdata) {
  int err = 0;
  hc_state_t *hc = (hc_state_t *)context;
  char buf[BUFSIZ];
  static int optimizer_done = 0; /* time before restart */

 IFVERBOSE printf("hc-opt\n");

 if(optimizer_done > 0) {
   optimizer_done--;
   /* restart hill climbing (in case we have better data now */
   if(!optimizer_done) {
     IFVERBOSE printf("restarting hill climb\n");
     hill_climb_cleanup(hc);
     hill_climb_init(hc, fdata->fd_perm, fdata->fd_po);
   }
   return 1;
 }

 while(!err) {
   //printf("hc\n");
   err = hill_climb_step(hc, fdata->fd_po, 
			 (evaluation_func_t)fexec_evaluate, fdata);
 }
 switch(err) {
 case RC_ERR_COMPLETE:
#ifdef VERBOSE
   printf("hill climb optimizer suggests: %s\n", 
	  pmPrint(hill_climb_result(hc), buf, BUFSIZ));
   fexec_print_cost(fdata, hill_climb_result(hc)); printf("\n");
#endif
   /* update and restart */
   optimizer_done = RESTART_INTERVAL;
   if(pmEqual(fdata->fd_perm, hill_climb_result(hc))) {
     IFVERBOSE printf("hill climbing didn't find further improvement\n");
     //hill_climb_cleanup(hc);
   } else {
     update_filter_order(fdata, hill_climb_result(hc));
     hill_climb_cleanup(hc);
     hill_climb_init(hc, fdata->fd_perm, fdata->fd_po);
   }
   break;
 case RC_ERR_NODATA:
   IFVERBOSE printf("hill climb optimizer needs more data for: %s\n", 
		    pmPrint(hill_climb_next(hc), buf, BUFSIZ));
   update_filter_order(fdata, hill_climb_next(hc));
   break;
 default:
   break;		
 }
 
 return (err == RC_ERR_COMPLETE);
}

/* ********************************************************************** */
/* indep policy */
/* ********************************************************************** */

#if 0

void *
indep_new(const filter_data_t *fdata) {
  bf_state_t *bf = (bf_state_t *)malloc(sizeof(bf_state_t));

  if(bf) {
    indep_init(bf, pmLength(fdata->fd_perm), fdata->fd_po, 
		    (evaluation_func_t)fexec_evaluate, fdata);
  }
#ifdef VERBOSE
  {
    char buf[BUFSIZ];
    printf("indep starts at: %s\n", pmPrint(fdata->fd_perm, buf, BUFSIZ));
  }
#endif
  return (void *)bf;
}

void
indep_delete(void *context) {
	bf_state_t *bf = (bf_state_t *)context;
	indep_cleanup(bf);
	free(bf);
}


int
indep_optimize(void *context, filter_data_t *fdata) {
	int err = 0;
	bf_state_t *bf = (bf_state_t *)context;
#ifdef VERBOSE
	char buf[BUFSIZ];
#endif
	static int optimizer_done = 0; /* time before restart */

	IFVERBOSE printf("bf-opt\n");

	if(optimizer_done > 0) {
		optimizer_done--;
		/* restart (in case we have better data now */
		if(!optimizer_done) {
		  IFVERBOSE printf("--- restarting optimizer ------------------------------\n");
		  indep_cleanup(bf);
		  indep_init(bf, pmLength(fdata->fd_perm), fdata->fd_po, 
				  (evaluation_func_t)fexec_evaluate, fdata);
		}
		return 1;
	}

	while(!err) {
		//printf("bf\n");
	  err = indep_step(bf);
	}
	switch(err) {
	case RC_ERR_COMPLETE:
#ifdef VERBOSE
		printf("optimizer done: %s\n", 
		       pmPrint(indep_result(bf), buf, BUFSIZ));
		fexec_print_cost(fdata, indep_result(bf)); printf("\n");
#endif
		/* update and restart */
		if(pmEqual(fdata->fd_perm, indep_result(bf))) {
		  IFVERBOSE printf("optimizer didn't find further improvement\n");
		  optimizer_done = RESTART_INTERVAL;
		  //indep_cleanup(bf);
		} else {
		  update_filter_order(fdata, indep_result(bf));
		  indep_cleanup(bf);
		  indep_init(bf, pmLength(fdata->fd_perm), fdata->fd_po, 
				  (evaluation_func_t)fexec_evaluate, fdata);
		}
		break;
	case RC_ERR_NODATA:
#ifdef VERBOSE
		printf("optimizer needs more data for: %s\n", 
		       pmPrint(indep_next(bf), buf, BUFSIZ));
		printf("\t"); fexec_print_cost(fdata, indep_next(bf)); printf("\n");
#endif
		update_filter_order(fdata, indep_next(bf));
		break;
	default:
		break;		
	}
		

	return (err == RC_ERR_COMPLETE);
}



#endif
