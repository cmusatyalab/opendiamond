
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
hill_climb_new(filter_data_t *fdata) {
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

void *
best_first_new(filter_data_t *fdata) {
  bf_state_t *bf = (bf_state_t *)malloc(sizeof(bf_state_t));

  if(bf) {
    best_first_init(bf, pmLength(fdata->fd_perm), fdata->fd_po, 
		    (evaluation_func_t)fexec_evaluate, fdata);
  }
#ifdef VERBOSE
  {
    char buf[BUFSIZ];
    printf("best_first starts at: %s\n", pmPrint(fdata->fd_perm, buf, BUFSIZ));
  }
#endif
  return (void *)bf;
}

void
best_first_delete(void *context) {
	bf_state_t *bf = (bf_state_t *)context;
	best_first_cleanup(bf);
	free(bf);
}


int
best_first_optimize(void *context, filter_data_t *fdata) {
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
		  best_first_cleanup(bf);
		  best_first_init(bf, pmLength(fdata->fd_perm), fdata->fd_po, 
				  (evaluation_func_t)fexec_evaluate, fdata);
		}
		return 1;
	}

	while(!err) {
		//printf("bf\n");
	  err = best_first_step(bf);
	}
	switch(err) {
	case RC_ERR_COMPLETE:
#ifdef VERBOSE
		printf("optimizer done: %s\n", 
		       pmPrint(best_first_result(bf), buf, BUFSIZ));
		fexec_print_cost(fdata, best_first_result(bf)); printf("\n");
#endif
		/* update and restart */
		if(pmEqual(fdata->fd_perm, best_first_result(bf))) {
		  IFVERBOSE printf("optimizer didn't find further improvement\n");
		  optimizer_done = RESTART_INTERVAL;
		  //best_first_cleanup(bf);
		} else {
		  update_filter_order(fdata, best_first_result(bf));
		  best_first_cleanup(bf);
		  best_first_init(bf, pmLength(fdata->fd_perm), fdata->fd_po, 
				  (evaluation_func_t)fexec_evaluate, fdata);
		}
		break;
	case RC_ERR_NODATA:
#ifdef VERBOSE
		printf("optimizer needs more data for: %s\n", 
		       pmPrint(best_first_next(bf), buf, BUFSIZ));
		printf("\t"); fexec_print_cost(fdata, best_first_next(bf)); printf("\n");
#endif
		update_filter_order(fdata, best_first_next(bf));
		break;
	default:
		break;		
	}
		

	return (err == RC_ERR_COMPLETE);
}


/* ********************************************************************** */
/* indep policy */
/* ********************************************************************** */


void *
indep_new(filter_data_t *fdata) {
  bf_state_t *bf = (bf_state_t *)malloc(sizeof(bf_state_t));

  if(bf) {
    best_first_init(bf, pmLength(fdata->fd_perm), fdata->fd_po, 
		    (evaluation_func_t)fexec_evaluate, fdata);
  }
#ifdef VERBOSE
  {
    char buf[BUFSIZ];
    printf("best_first starts at: %s\n", pmPrint(fdata->fd_perm, buf, BUFSIZ));
  }
#endif
  return (void *)bf;
}


void *
x_indep_new(filter_data_t *fdata) {
  indep_state_t *iSt = (indep_state_t *)malloc(sizeof(indep_state_t));

  if(iSt) {
    indep_init(iSt, pmLength(fdata->fd_perm), fdata->fd_po, 
		    (evaluation_func_t)fexec_evaluate, fdata);
  }
#ifdef VERBOSE
  {
    char buf[BUFSIZ];
    printf("indep starts at: %s\n", pmPrint(fdata->fd_perm, buf, BUFSIZ));
  }
#endif
  return (void *)iSt;
}

void
indep_delete(void *context) {
	indep_state_t *iSt = (indep_state_t *)context;
	indep_cleanup(iSt);
	free(iSt);
}


int
indep_optimize(void *context, filter_data_t *fdata) {
	int err = 0;
	indep_state_t *iSt = (indep_state_t *)context;
#ifdef VERBOSE
	char buf[BUFSIZ];
#endif
	static int optimizer_done = 0; /* time before restart */

	IFVERBOSE printf("iSt-opt\n");

	if(optimizer_done > 0) {
		optimizer_done--;
		/* restart (in case we have better data now */
		if(!optimizer_done) {
		  IFVERBOSE printf("--- restarting optimizer ------------------------------\n");
		  indep_cleanup(iSt);
		  indep_init(iSt, pmLength(fdata->fd_perm), fdata->fd_po, 
				  (evaluation_func_t)fexec_evaluate, fdata);
		}
		return 1;
	}

	while(!err) {
		//printf("iSt\n");
	  err = indep_step(iSt);
	}
	switch(err) {
	case RC_ERR_COMPLETE:
#ifdef VERBOSE
		printf("optimizer done: %s\n", 
		       pmPrint(indep_result(iSt), buf, BUFSIZ));
		fexec_print_cost(fdata, indep_result(iSt)); printf("\n");
#endif
		/* update and restart */
		if(pmEqual(fdata->fd_perm, indep_result(iSt))) {
		  IFVERBOSE printf("optimizer didn't find further improvement\n");
		  optimizer_done = RESTART_INTERVAL;
		  //indep_cleanup(iSt);
		} else {
		  update_filter_order(fdata, indep_result(iSt));
		  indep_cleanup(iSt);
		  indep_init(iSt, pmLength(fdata->fd_perm), fdata->fd_po, 
				  (evaluation_func_t)fexec_evaluate, fdata);
		}
		break;
	case RC_ERR_NODATA:
#ifdef VERBOSE
		printf("optimizer needs more data for: %s\n", 
		       pmPrint(indep_next(iSt), buf, BUFSIZ));
		printf("\t"); fexec_print_cost(fdata, indep_next(iSt)); printf("\n");
#endif
		update_filter_order(fdata, indep_next(iSt));
		break;
	default:
		break;		
	}
		

	return (err == RC_ERR_COMPLETE);
}



/* ********************************************************************** */


void *
random_new(filter_data_t *fdata) {
  permutation_t *perm;

  perm = pmDup(fdata->fd_perm);
  randomize_permutation(perm, fdata->fd_po);
  update_filter_order(fdata, perm);
  pmDelete(perm);

#ifdef VERBOSE
  {
    char buf[BUFSIZ];
    printf("random starts at: %s\n", pmPrint(fdata->fd_perm, buf, BUFSIZ));
  }
#endif

  return NULL;
}


int
random_optimize(void *context, filter_data_t *fdata) {
#ifdef VERBOSE
  char buf[BUFSIZ];
#endif
  static int optimizer_done = 0; /* time before restart */

  IFVERBOSE printf("iSt-opt\n");

  if(optimizer_done > 0) {
    optimizer_done--;
  }

  /* restart (in case we have better data now */
  if(!optimizer_done) {
    permutation_t *perm;

    IFVERBOSE printf("--- restarting optimizer ------------------------------\n");
    
    perm = pmDup(fdata->fd_perm);
    randomize_permutation(perm, fdata->fd_po);
    update_filter_order(fdata, perm);
    pmDelete(perm);
    optimizer_done = RESTART_INTERVAL;
  }

  return RC_ERR_COMPLETE;
}



/* ********************************************************************** */

void *
static_new(filter_data_t *fdata) {

  /* use a fixed permutation -- XXX -- figure out how to set it here */

#ifdef VERBOSE
  {
    char buf[BUFSIZ];
    printf("static starts at: %s\n", pmPrint(fdata->fd_perm, buf, BUFSIZ));
  }
#endif

  return NULL;
}
