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

#ifndef	_LIB_LOG_H_
#define	_LIB_LOG_H_	1



/*
 * These are the type definitions that are used to keep track
 * of the different senders of log messages.  These are kept as
 * a bitmask so they advance in powers of 2.
 */

#define	LOGT_APP	0x00000001	/* from application */ 
#define	LOGT_DISK	0x00000002	/* from emulated disk */
#define	LOGT_FILT	0x00000004	/* from filter evaluation */
#define	LOGT_BG		0x00000008	/* from host background process */
#define LOGT_UTILITY    0x00000010      /* from generic utility functions */
#define	LOGT_ALL	0xFFFFFFFF	/* log all types */

/*
 * These are the different log levels that can be defined for finding
 * out what application are logging.   These are also bit masks.
 */
#define	LOGL_CRIT	0x00000001	/* A critical error   */
#define	LOGL_ERR	0x00000002	/* an error condition */
#define	LOGL_INFO	0x00000004	/* General Information */
#define	LOGL_TRACE	0x00000008	/* Tracing information for debugging */
#define	LOGL_ALL	0xFFFFFFFF	/* log all levels */


typedef struct log_ent {
	uint32_t 	le_level;	/* the level */
	uint32_t 	le_type;	/* the type */
	uint32_t 	le_dlen;	/* length of data */
	uint32_t 	le_nextoff;	/* off set of the next record */
	char		le_data[1];	/* where the string is stored */
} log_ent_t;


#ifndef offsetof
#define offsetof(type, member) ( (int) & ((type*)0) -> member )
#endif

#define	LOG_ENT_BASE_SIZE	(offsetof(struct log_ent, le_data))


/*
 * Defined the maximum message size for any given message.  Anything
 * longer than this will be truncated.
 */
#define	MAX_LOG_ENTRY	128
#define	MAX_LOG_STRING	(MAX_LOG_ENTRY - LOG_ENT_BASE_SIZE)

/*
 * The total amount of space the we should buffer for logging.
 */
#define	MAX_LOG_BUFFER	128*1024



#ifdef __cplusplus
extern "C" {
#endif

void log_init(void **log_cookie);
void log_thread_register(void *log_cookie);
void log_setlevel(unsigned int level_mask);
void log_settype(unsigned int type_mask);
int  log_getdrops();
void log_message(unsigned int type, unsigned int level, char *fmt, ...);
int  log_getbuf(char **data);
void  log_advbuf(int len);



#ifdef __cplusplus
}
#endif

#endif	/* !defined(_LIB_LOG_H_) */

