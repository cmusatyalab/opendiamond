/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 4
 *
 *  Copyright (c) 2002-2005 Intel Corporation
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#ifndef	_LIB_LOG_H_
#define	_LIB_LOG_H_	1

#include <stdint.h>
#include <pthread.h>

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
#define LOGT_NET	0x00000020      /* network related */
#define	LOGT_ALL	0xFFFFFFFF	/* log all types */

/*
 * These are the different log levels that can be defined for finding
 * out what application are logging.   These are also bit masks.
 */
#define	LOGL_CRIT	0x00000001	/* A critical error   */
#define	LOGL_ERR	0x00000002	/* an error condition */
#define	LOGL_INFO	0x00000004	/* General Information */
#define	LOGL_TRACE	0x00000008	/* Tracing information for analysis */
#define LOGL_DEBUG  0x00000010  /* Debugging */
#define	LOGL_ALL	0xFFFFFFFF	/* log all levels */

/*
 * Defined the maximum message size for any given message.  Anything
 * longer than this will be truncated.
 */
#define	MAX_LOG_ENTRY	256

/*
 * Define the maximum log file size.  The log file is rolled
 * before the limit is exceeded.
 */
#define MAX_LOG_FILE_SIZE 1000000

#ifdef __cplusplus
extern "C"
{
#endif

void log_init(const char *prefix, const char *control);

void log_term(void);

void log_setlevel(unsigned int level_mask);

void log_settype(unsigned int type_mask);

void log_message(unsigned int type, unsigned int level, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif	/* !defined(_LIB_LOG_H_) */

