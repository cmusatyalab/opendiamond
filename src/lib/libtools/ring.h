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

#ifndef	_RING_H_
#define	_RING_H_


#ifdef __cplusplus
extern          "C" {
#endif


    /*
     * This number of elements in a ring.  This must be a multiple of 2. 
     */

    typedef enum {
        RING_TYPE_SINGLE,
        RING_TYPE_DOUBLE
    } ring_type_t;

    typedef struct ring_data {
        pthread_mutex_t mutex;  /* make sure updates are atomic */
        ring_type_t     type;   /* is it 1 or 2 element ring ? */
        int             head;   /* location for next enq */
        int             tail;   /* location for next deq */
        int             size;   /* total number of elements */
        void           *data[0];
    } ring_data_t;


#define	RING_STORAGE_SZ(__num_elem)	(sizeof(ring_data_t) + (((__num_elem) + 2) \
					 * sizeof(void *)))


    /*
     * These are the function prototypes that are used for
     * single entries.
     */
    int             ring_init(ring_data_t ** ring, int num_elem);
    int             ring_empty(ring_data_t * ring);
    int             ring_full(ring_data_t * ring);
    int             ring_enq(ring_data_t * ring, void *data);
    int             ring_count(ring_data_t * ring);
    void           *ring_deq(ring_data_t * ring);

    /*
     * These are the function prototypes for the double entry functions.
     */
    int             ring_2init(ring_data_t ** ring, int num_elem);
    int             ring_2empty(ring_data_t * ring);
    int             ring_2full(ring_data_t * ring);
    int             ring_2enq(ring_data_t * ring, void *data1, void *data2);
    int             ring_2count(ring_data_t * ring);
    int             ring_2deq(ring_data_t * ring, void **data1, void **data2);

#ifdef __cplusplus
}
#endif
#endif                          /* _RING_H_ */
