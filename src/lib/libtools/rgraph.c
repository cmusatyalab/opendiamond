
/* 
 * Rajiv Wickremesinghe 2/2003
 */


#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <assert.h>
#include "rgraph.h"

//#define VERBOSE 1

/* ********************************************************************** */


static void
vec_init(edgelist_t *vec) {
  vec->len = 0;
  vec->size = 0;
  vec->edges = NULL;
}

static void
vec_free(edgelist_t *vec) {
  if(vec->edges) free(vec->edges);
}

static edge_t *
vec_append(edgelist_t *vec) {
  
  if(vec->len == vec->size) {
    vec->size += 32;		/* linear growth */
    vec->edges = realloc(vec->edges, vec->size);
  }
  return &vec->edges[vec->len++];
}


static void
sort_edgelist(edgelist_t *vec) {
  int i, j;
  edge_t v;

  /* insertion sort */
  for(i=1; i < vec->len; i++) {
    v = vec->edges[i];
    j = i;
    while(j > 0 && vec->edges[j-1].eg_val > v.eg_val) {
      vec->edges[j] = vec->edges[j-1];
      j--;
    }
    vec->edges[j] = v;
  }
}

static edge_t *
vec_next(edgelist_t *vec, edge_t *ep) {
  assert(ep >= vec->edges);
  ep++;
  return (ep < vec->edges + vec->len ? ep : NULL);
}

static edge_t *
vec_first(edgelist_t *vec) {
  return (vec->len ? vec->edges : NULL);
}

#define VEC_FOREACH(ep,vec) \
  for(ep = vec_first(vec); (ep); ep = vec_next(vec, ep))

/* ********************************************************************** */

void
gInit(graph_t *g) {
  TAILQ_INIT(&g->nodes);
  g->current_id = 1;
}


void
gInitFromList(graph_t *g, graph_t *src) {
  node_t *prev=NULL, *node, *np;

  gInit(g);
  GLIST(src, np) {
    node = gNewNode(g, np->label);
    node->val = np->val;	/* XXX */

    if(prev) gAddEdge(g, prev, node);
    prev = node;
  }
}

void
gClear(graph_t *g) {
  node_t *np;

  /* travese node list */
  while(!TAILQ_EMPTY(&g->nodes)) {
    np = TAILQ_FIRST(&g->nodes);
    TAILQ_REMOVE(&g->nodes, np, link);

    /* free edges */
    vec_free(&np->edgelist);
/*     while(!TAILQ_EMPTY(&np->edges)) { */
/*       ep = TAILQ_FIRST(&np->edges); */
/*       TAILQ_REMOVE(&np->edges, ep, eg_link); */
/*       free(ep); */
/*     }   */
    free(np);
  }  
}

node_t *
gNewNode(graph_t *g, char *label) {
  node_t *np;
  np = (node_t *)malloc(sizeof(node_t));
  assert(np);
  np->id = g->current_id++;
  np->label = strdup(label);
  //TAILQ_INIT(&np->edges);
  vec_init(&np->edgelist);

  TAILQ_INSERT_TAIL(&g->nodes, np, link);
  return np;
}
  

void
gAddEdge(graph_t *g, node_t *u, node_t *v) {
  edge_t *ep;

/*   ep = (edge_t *)malloc(sizeof(edge_t)); */
/*   assert(ep); */
/*   ep->eg_v = v; */

  //TAILQ_INSERT_TAIL(&u->edges, ep, eg_link);
  ep = vec_append(&u->edgelist);
  ep->eg_v = v;
  ep->eg_val = v->val;		/* XXX */
}

void
gRemoveEdge(graph_t *g, edge_t *ep) {

  assert(0);
  
  free(ep);
}



void
gPrintNode(node_t *np) {
  edge_t *ep;

  printf("{<%s>, ", np->label);
  printf("%d, ", np->val);

  printf("E(");
  //TAILQ_FOREACH(ep, &np->edges, eg_link) {
  VEC_FOREACH(ep, &np->edgelist) {
    printf(" <%s>", ep->eg_v->label);
  }
  printf(" )}\n");
}


/* ********************************************************************** */

#define print_nodelist(list,link) 		\
{						\
  node_t *np;					\
  TAILQ_FOREACH(np, (list), link) {		\
    gPrintNode(np);				\
  }						\
}

void
gPrint(graph_t *g) {
  //printf("nodes:\n");
  print_nodelist(&g->nodes, link);
}


/* ********************************************************************** */

#if 0
int
dfs_visit(node_t *np, int *time) {
  int loop = 0;
  edge_t *ep;

  switch(np->color) {
  case 0:
    np->td = ++(*time);
    np->color = 1;
    TAILQ_FOREACH(ep, &np->edges, eg_link) {
      loop += dfs_visit(ep->eg_v, time);
    }
    np->te = ++(*time);
    break;
  case 1:
    loop = 1;
    break;
  case 2:
    break;
  default:
    assert(0);
  }
  return loop;
}


void
gDFS(graph_t *g) {
  node_t *np;
  int time=0;
  int loop = 0;

  TAILQ_FOREACH(np, &g->nodes, link) {
    np->color = 0;
  }
  TAILQ_FOREACH(np, &g->nodes, link) {
    loop = dfs_visit(np, &time);
  }
  if(loop) {
    fprintf(stderr, "loop detected\n");
  }
}
#endif

/* ********************************************************************** */

static int
topo_visit(node_t *np, nodelist_t *list) {
  int loop = 0;
  edge_t *ep;

  if(np->color == 1) return 1;	/* loop */

  if(np->color == 2) return 0;	/* bottom of recursion */

  np->color = 1;
#ifdef VERBOSE
  fprintf(stderr, "visiting %s\n", np->label);
#endif
  //TAILQ_FOREACH(ep, &np->edges, eg_link) {
  sort_edgelist(&np->edgelist);	/* XXX */
  VEC_FOREACH(ep, &np->edgelist) {
    loop += topo_visit(ep->eg_v, list);
  }
  np->color = 2;
  if(loop) return loop;

  /* finished this node, so all descendants are done. safe to add to done list. */
#ifdef VERBOSE
  fprintf(stderr, "added %s\n", np->label);
#endif
  TAILQ_INSERT_HEAD(list, np, olink);

  return 0;
}


/* basically does DFS. nodes that are completed are put on the done
 * list (g->olist) */
int
gTopoSort(graph_t *g) {
  node_t *np;
  int loop = 0;

  /* init */
  TAILQ_INIT(&g->olist);
  TAILQ_FOREACH(np, &g->nodes, link) {
    np->color = 0;
  }

  /* visit all nodes */
  TAILQ_FOREACH(np, &g->nodes, link) {
    if(np->color == 0) loop += topo_visit(np, &g->olist);
  }

  if(loop) {
    fprintf(stderr, "loop detected while doing TopoSort\n");
    return 1;
  }

  //printf("topo list:\n");
  //print_nodelist(&g->olist, olink);
  return 0;
}


/* ********************************************************************** */

/* a priority queue implemented as a list. heap is a pain to maintain
 * because of the back pointers.
 */

/* keep list in DECREASING order */

struct pe_t;
typedef struct pe_t {
  node_t *node;			/* could be void */
  int prio;
  TAILQ_ENTRY(pe_t) link;
} pe_t;

typedef TAILQ_HEAD(pe_list_t, pe_t) pe_list_t;

void
pInit(pe_list_t *list) {
  TAILQ_INIT(list);
}

void
pClean(pe_list_t *list) {
  pe_t *np;

  while(!TAILQ_EMPTY(list)) {
    np = TAILQ_FIRST(list);
    TAILQ_REMOVE(list, np, link);
    free(np);
  }
}


pe_t *
pInsert(pe_list_t *list, node_t *node, int prio) {
  pe_t *np;
  pe_t *cur;

  np = (pe_t*)malloc(sizeof(pe_t));
  assert(np);
  np->node = node;
  np->prio = prio;

  TAILQ_FOREACH(cur, list, link) {
    if(cur->prio <= prio) {
      TAILQ_INSERT_BEFORE(cur, np, link);
      return np;
    }
  }
  TAILQ_INSERT_TAIL(list, np, link);  
  return np;
}


node_t *
pDeleteMin(pe_list_t *list) {
  pe_t *np;
  node_t *node;

  np = TAILQ_LAST(list, pe_list_t);
  TAILQ_REMOVE(list, np, link);
  node = np->node;
  free(np);

  return node;
}


void
pReduce(pe_list_t *list, pe_t *np, int prio) {
  pe_t *next;

  np->prio = prio;
  next = TAILQ_NEXT(np, link);
  TAILQ_REMOVE(list, np, link);
  while(next && next->prio > prio) {
    next = TAILQ_NEXT(next, link);    
  }    
  if(next) {
    TAILQ_INSERT_BEFORE(next, np, link);
  } else {
    TAILQ_INSERT_TAIL(list, np, link);
  }
}

int
pIsEmpty(pe_list_t *list) {
  return TAILQ_EMPTY(list);
}

void
pPrint(pe_list_t *list) {
  pe_t *np;
  
  TAILQ_FOREACH(np, list, link) {
    printf(" <%s,%d>", np->node->label, np->prio);
  }
  printf("\n");
}



/* ********************************************************************** */

void
gSSSP(graph_t *g, node_t *src) {
  node_t *np;
  edge_t *ep;
  pe_list_t pq;

  pInit(&pq);

  TAILQ_FOREACH(np, &g->nodes, link) {
    np->td = INT_MAX;		/* inf depth */
    np->color = 0;		/* not visited */
    np->pqe = pInsert(&pq, np, INT_MAX);
  }
  src->td = 0;
  pReduce(&pq, src->pqe, 0);
  
  while(!pIsEmpty(&pq)) {
    pPrint(&pq);
    np = pDeleteMin(&pq);
    printf("processing node %s\n", np->label);
    np->color = 1;
    //TAILQ_FOREACH(ep, &np->edges, eg_link) {   
    VEC_FOREACH(ep, &np->edgelist) {
      node_t *v = ep->eg_v;	/* alias */
      if(v->color == 0) {
	if(v->td > np->td + v->val) {
	  assert(v->color == 0);
	  printf("touching %s\n", v->label);
	  v->td = np->td + v->val;
	  pReduce(&pq, v->pqe, v->td);
	  v->visit = np;
	}
      }
    }
  }

  TAILQ_FOREACH(np, &g->nodes, link) {    
    printf("%s - %d\n", np->label, np->td);
  }

}


/* ********************************************************************** */

static void
export_node(FILE *fp, node_t *np, int indent) {
  edge_t *ep;
  int count = 0;
  char *edgecolor;
  char *red = "red";
  char *black = "black";

  if(np->color) return;
    
  np->color = 1;

  fprintf(fp, "l(\"%d\",n(\"A\",", np->id);
  //fprintf(fp, "[a(\"OBJECT\",\"%s\")],\n", np->label);
  fprintf(fp, "[a(\"OBJECT\",\"%s\\n(%d)\")", np->label, np->val);
  //fprintf(fp, ",\na(\"COLOR\",\"red\")");
  fprintf(fp, "],\n");

  /* export edges */
  fprintf(fp, "\t[");
  //TAILQ_FOREACH(ep, &np->edges, eg_link) {
  VEC_FOREACH(ep, &np->edgelist) {
    if(count++) fprintf(fp, ",\n\t");

    edgecolor = (TAILQ_NEXT(np, olink) == ep->eg_v) ? red : black;

    fprintf(fp, "l(\"%d-%d\",e(\"B\",[a(\"EDGECOLOR\",\"%s\")],r(\"%d\")))",
	    np->id, ep->eg_v->id, edgecolor, ep->eg_v->id);
    //export_node(fp, ep->node, indent+1);    
  }
  fprintf(fp, "\n\t]))\n");
}


/* export in daVinci format */
void
gExport(graph_t *g, char *filename) {
  node_t *np;
  int indent = 0;
  FILE *fp;
  int count = 0;

  fp = fopen(filename, "w");
  if(!fp) {
    perror(filename);
    exit(1);
  }

  TAILQ_FOREACH(np, &g->nodes, link) {
    np->color = 0;
  }
  fprintf(fp, "[\n");
  TAILQ_FOREACH(np, &g->nodes, link) {
    if(count++) fprintf(fp, ",");
    fprintf(fp, "\n");
    export_node(fp, np, indent);
  }
  fprintf(fp, "]\n");

  fclose(fp);
}

/* ********************************************************************** */
