
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "rcomb.h"

/* ---------------------------------------------------------------------- */

permutation_t *
pmNew(int len) {
  permutation_t *ptr;

  ptr = (permutation_t *)malloc(sizeof(permutation_t) +
				sizeof(int) * len);
  assert(ptr);
  ptr->length = len;

  return ptr;
}

void
pmDelete(permutation_t *ptr) {
  if(ptr) {
    free(ptr);
  }
}


void
pmCopy(permutation_t *copy, const permutation_t *ptr) {
  int i;

  copy->length = ptr->length;
  for(i=0; i<ptr->length; i++) {
    copy->elements[i] = ptr->elements[i];
  }
}

permutation_t *
pmDup(const permutation_t *ptr) {
  permutation_t *copy;

  copy = pmNew(ptr->length);
  pmCopy(copy, ptr);
  return copy;
}

int
pmElt(const permutation_t *pm, int i) {
  return pm->elements[i];
}

void
pmSetElt(permutation_t *pm, int i, int val) {
  pm->elements[i] = val;
}


int
pmLength(const permutation_t *ptr) {
  return ptr->length;
}

void
pmSwap(permutation_t *ptr, int i, int j) {
  int tmp;
  tmp = ptr->elements[i];
  ptr->elements[i] = ptr->elements[j];
  ptr->elements[j] = tmp;
}

int
pmEqual(permutation_t *pm1, permutation_t *pm2) {
  int i;

  if(pm1->length != pm2->length) {
    return 0;
  }
  for(i=0; i<pm1->length; i++) {
    if(pm1->elements[i] != pm2->elements[i]) {
      return 0;
    }
  }
  return 1;
}


char *
pmPrint(const permutation_t *pm, char *buf, int bufsiz) {
  int i;
  char buf2[BUFSIZ];

  buf[0] = '\0';
  for(i=0; i<pm->length; i++) {
    sprintf(buf2, "%s%d", (i?" ":""), pm->elements[i]);
    if(strlen(buf) + strlen(buf2) > bufsiz) {
      break;
    }
    strcat(buf, buf2);
  }
  return buf;
}

/* ---------------------------------------------------------------------- */
/* poset functions */


partial_order_t *
poNew(int n) {
  partial_order_t *po;

  po = (partial_order_t *)malloc(sizeof(partial_order_t) +
				 n * n * sizeof(char));
  assert(po);
  po->dim = n;
  memset(po->data, PO_INCOMPARABLE, n * n * sizeof(char));

  return po;
}

void
poDelete(partial_order_t *po) {
  if(po) {
    free(po);
  }
}

static po_relation_t
poInverse(po_relation_t rel) {
  return (rel == PO_INCOMPARABLE ? rel : -rel);
}
    

void
poSetOrder(partial_order_t *po, int u, int v, po_relation_t rel) {
  po->data[u * po->dim + v] = rel;
  po->data[v * po->dim + u] = poInverse(rel);
}


int
poIncomparable(const partial_order_t *po, int u, int v) {
  char cmp;

  assert(u < po->dim);
  assert(v < po->dim);
  cmp = po->data[u * po->dim + v];
  return (cmp == PO_INCOMPARABLE);
}

/* ---------------------------------------------------------------------- */



/* some permutation algorithms adapted from the Perl versions by Rahul S.
 * Rajiv Wickremesinghe 5/2003
 */



#define SWAP_PTR(p1,p2)				\
{						\
  void *tmp;					\
  tmp = p1;					\
  p1 = p2;					\
  p2 = tmp;					\
}




/* 
 * single iteration of hill climbing.
 * returns a new permutation
 */

void
hill_climb_init(hc_state_t *ptr, const permutation_t *start) {
  ptr->best_seq = pmDup(start);
  ptr->next_seq = pmNew(pmLength(start));
  ptr->n = pmLength(start);
  ptr->i = 0;
  ptr->j = 1;
  ptr->improved = 1;
}

void
hill_climb_cleanup(hc_state_t *ptr) {
  pmDelete(ptr->best_seq);
  pmDelete(ptr->next_seq);
}


/* we have the option of exhaustively looking at all the neighboring
 * permutations and picking the best, or we could just greedily pick
 * the first improving one.. */

int
hill_climb_step(hc_state_t *hc, const partial_order_t *po, 
		evaluation_func_t evf, void *context) {
  int i,j;
  int err=0;
  int n = hc->n;
  int best_score;
	int next_score;
  char buf[BUFSIZ];

  /* evaluate (re-evaluate?) current state */
  err = evf(context, hc->best_seq, &best_score);

  while(!err && hc->improved) {
    printf("best: %s, score=%d\n", pmPrint(hc->best_seq, buf, BUFSIZ),
	   best_score);

    /* test seq */
    pmCopy(hc->next_seq, hc->best_seq);
    
    i = hc->i;
    j = hc->j;

    while(i < n-1) {
      //printf("best=%s, i=%d, j=%d\n", pmPrint(hc->best_seq, buf, BUFSIZ), i, j);
      /* check if this is a valid swap. if there is a partial order, we can't swap */
      /* next_seq == best_seq at this point */
      if(poIncomparable(po, pmElt(hc->next_seq, i), pmElt(hc->next_seq, j))) {

	/* generate swapped perm, evaluate */
	pmSwap(hc->next_seq, i, j);
	
	/* evaluate option */
	err = evf(context, hc->next_seq, &next_score);
	if(err) {
	  goto done;
	}

	printf("permutation %d/%d: %s, score=%d\n", i, j, pmPrint(hc->next_seq, buf, BUFSIZ),
	       next_score);

	/* keep track of best */
	if(next_score > best_score) {
	  printf("improved!\n");
	  hc->improved = 1;
	  best_score = next_score;
	  pmCopy(hc->best_seq, hc->next_seq);
	}
	/* swap back to generate original (cheaper than copy) */
	pmSwap(hc->next_seq, i, j);
      }

      /* update loop */
      j++;
      if(j>=n) {
	i++;
	j = i+1;
      }
    } /* while(i.. */

    /* reset loop */
    i = 0;
    j = 1;
    hc->improved = 0;

  done:
    hc->i = i;
    hc->j = j;
  }
  
  /* check if we are done */
  if(!err && hc->improved == 0) {
    err = RC_ERR_COMPLETE;
  }

  return err;
}

permutation_t *
hill_climb_result(hc_state_t *hc) {
  return hc->best_seq;
}

permutation_t *
hill_climb_next(hc_state_t *hc) {
  return hc->next_seq;
}
