/*
 *
 *
 *                          Diamond 1.0
 * 
 *            Copyright (c) 2002-2004, Intel Corporation
 *                         All Rights Reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 *    * Neither the name of Intel nor the names of its contributors may
 *      be used to endorse or promote products derived from this software 
 *      without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef _RGRAPH_H_
#define _RGRAPH_H_

/*
 * graph abstraction and some algorithms on graphs. currently uses an
 * edge-list representation.
 *
 * Rajiv Wickremesinghe 2/2003
 */

#include "queue.h"

#ifdef __cplusplus
extern          "C" {
#endif

#if 0
}
#endif
struct node_t;
typedef struct edge_t {
    struct node_t  *eg_v;
    // TAILQ_ENTRY(edge_t) eg_link;

    /*
     * user's data 
     */
    int             eg_val;
    int             eg_color;
} edge_t;


struct node_t;

typedef
TAILQ_HEAD(nodelist_t, node_t)
    nodelist_t;

// typedef TAILQ_HEAD(edgelist_t, edge_t) edgelist_t;
    typedef struct edgelist_t {
        int             len;
        int             size;
        edge_t         *edges;
    } edgelist_t;



    typedef struct node_t {
        /*
         * temp data used/output by algorithms 
         */
        int             color;
        int             td;     /* discovery time / depth etc. */
        int             te;     /* end time */
                        TAILQ_ENTRY(node_t) olink;  /* ordered link */
        struct node_t  *visit;  /* */
        void           *pqe;    /* pq entry */


        /*
         * internal state 
         */
        int             id;     /* unique id */
        char           *label;  /* printable label */
                        TAILQ_ENTRY(node_t) link;   /* link for node list */
        edgelist_t      successors; /* list of edges */
        edgelist_t      predecessors;   /* list of edges */
                        TAILQ_ENTRY(node_t) clink;  /* link for cluster list */

        /*
         * user's data 
         */
        int             val;
        void           *data;
    } node_t;




    typedef struct graph_t {
        nodelist_t      nodes;
        nodelist_t      olist;  /* alternate list (output of some funcs) see
                                 * GLIST */
        int             current_id; /* also an upper bound on no. of nodes */
    } graph_t;


/*
 * initialize structs 
 */
    void            gInit(graph_t * g);

/*
 * initialize a graph from the olist of the src graph. 
 */
    void            gInitFromList(graph_t * g, graph_t * src);

/*
 * deallocate (does not free g) 
 */
    void            gClear(graph_t * g);

/*
 * allocate and add a new unconnected to graph 
 */
    node_t         *gNewNode(graph_t * g, char *label);

/*
 * add a directed edge to the graph 
 */
    edge_t         *gAddEdge(graph_t * g, node_t * u, node_t * v);

    const edgelist_t *gSuccessors(const graph_t * g, node_t * u);
    const edgelist_t *gPredecessors(const graph_t * g, node_t * u);

/*
 * print to stdout 
 */
    void            gPrint(graph_t * g);
    void            gPrintNode(node_t * np);

/*
 * export in daVinci format to filename 
 */
    void            gExport(graph_t * g, char *filename);

/*
 * a macro to visit all nodes. eg: GFOREACH(graph, nodeptr) {
 * gPrintNode(nodeptr); } 
 */
#define GFOREACH(g,np)  TAILQ_FOREACH((np), &(g)->nodes, link)

/*
 * a macro to use the list output 
 */
#define GLIST(g,np)  TAILQ_FOREACH((np), &(g)->olist, olink)


/*
 * algorithms
 */

/*
 * do topological sort. output will be in olist. should probably make a new
 * graph for orthogonality, but... 
 */
    int             gTopoSort(graph_t * g);

/*
 * run Dijkstra. node weights read from node->val. output (depth) will be in
 * node->td, SP can be found via node->visit 
 */
    void            gSSSP(graph_t * g, node_t * src);


    void            gAssignClusters(graph_t * g, nodelist_t **, int *);


#ifdef __cplusplus
}
#endif

#endif                          /* _RGRAPH_H_ */
