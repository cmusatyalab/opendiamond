/*
 * 	Diamond (Release 1.0)
 *      A system for interactive brute-force search
 *
 *      Copyright (c) 2002-2005, Intel Corporation
 *      All Rights Reserved
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
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

