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


#ifndef	_OCACHE_PRIV_H_
#define	_OCACHE_PRIV_H_ 	1


struct ocache_state;


#define	MAX_DIR_PATH	128
#define	MAX_GID_NAME	128

#define	CACHE_EXT		"CACHEFL"
#define CACHE_DIR		"cache"
#define CACHE_OATTR_EXT		"OATTR"

#define	MAX_GID_FILTER	64
/*
 * XXX we need to clean up this interface so this is not externally 
 * visible.
 */
typedef struct ocache_state
{
	char		ocache_path[MAX_DIR_PATH];
	pthread_t	c_thread_id;   // thread for cache table
	pthread_t	o_thread_id;   // thread for output attrs
	void *		dctl_cookie;
	void *		log_cookie;
}
ocache_state_t;


#endif	/* !_OCACHE_PRIV_H_ */

