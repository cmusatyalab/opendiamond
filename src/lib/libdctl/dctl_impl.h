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

#ifndef	_DCTL_IMPL_H_
#define	_DCTL_IMPL_H_	1



typedef struct dctl_leaf {
	LIST_ENTRY(dctl_leaf)	leaf_link;
	char *			        leaf_name;
	dctl_data_type_t	    leaf_type;
	dctl_write_fn		    leaf_write_cb;
	dctl_read_fn		    leaf_read_cb;
	void *			        leaf_cookie;
} dctl_leaf_t;

typedef enum {
        DCTL_NODE_LOCAL,
        DCTL_NODE_FWD,
} dctl_node_type_t;



typedef struct dctl_node {
    dctl_node_type_t                    node_type;
	LIST_ENTRY(dctl_node)	            node_link;
	LIST_HEAD(node_children, dctl_node) node_children;
	LIST_HEAD(node_leafs, dctl_leaf)    node_leafs;
	char *			                    node_name;
    void *                              node_cookie;
    dctl_fwd_rleaf_fn                   node_rleaf_cb;
    dctl_fwd_wleaf_fn                   node_wleaf_cb;
    dctl_fwd_lnodes_fn                  node_lnodes_cb;
    dctl_fwd_lleafs_fn                  node_lleafs_cb;
} dctl_node_t;

	
#endif	/* !defined(_DCTL_IMPL_H_) */


