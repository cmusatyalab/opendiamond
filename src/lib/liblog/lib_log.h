#ifndef	_LIB_LOG_H_
#define	_LIB_LOG_H_	1



/*
 * These are the type definitions that are used to keep track
 * of the different senders of log messages.  These are kept as
 * a bitmask so they advance in powers of 2.
 */

#define	LOGT_APP	0x00000001	/* from application */ 
#define	LOGT_VDISK	0x00000002	/* from emulated disk */
#define	LOGT_FILT	0x00000004	/* from filter valuation */
#define	LOGT_BG		0x00000008	/* from host background process */
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
	unsigned int	le_level;	/* the level */
	unsigned int	le_type;	/* the type */
	unsigned int	le_dlen;	/* length of data */
	unsigned int	le_nextoff;	/* off set of the next record */
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



extern void log_init();
extern void log_setlevel(unsigned int level_mask);
extern void log_settype(unsigned int type_mask);
extern int  log_getdrops();
extern void log_message(unsigned int type, unsigned int level, char *fmt, ...);
extern int  log_getbuf(char **data);
extern void  log_advbuf(int len);


#endif	/* !defined(_LIB_LOG_H_) */


