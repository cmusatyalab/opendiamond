#ifndef _LIB_SEARCH_PRIV_H_
#define _LIB_SEARCH_PRIV_H_


/*
 * This is header file defines the internal state that is used
 * for the searchlet library.
 */


/*
 * This structure keeps track of the state associated with each
 * of the storage devices.
 */
#define	DEV_FLAG_RUNNING		0x01	
#define	DEV_FLAG_COMPLETE		0x02	
struct search_context;

typedef struct device_state {
	struct device_state * 	next;
	pthread_t		thread_id;
	ring_data_t *		device_ops;	
	unsigned int		flags;
	struct search_context * sc;
	void	*		data_cookie;
	int			ver_no;
} device_state_t;


typedef enum {
	SS_ACTIVE,		/* a search is currently in progress */
	SS_DONE,		/* search active, all object are processed */
	SS_EMPTY,
	SS_SHUTDOWN,	
	SS_IDLE	
} search_state_t;

/*
 * This is the state associated with the object
 */
typedef struct {
	off_t			data_len;
	off_t			cur_offset;
	int			cur_blocksize;
	char *			data;
	obj_attr_t		attr_info;
} obj_data_t;


typedef struct {
	obj_data_t *		obj;
	int			ver_num;
} obj_info_t;
/*
 * This defines the structures that keeps track of the current search
 * context.  This is the internal state that is kept for consistency,
 * etc.
 */

#define	OBJ_QUEUE_SIZE		1024
typedef struct search_context {
	int			cur_search_id;	/* ID of current search */
	struct device_state *	dev_list;
	search_state_t		cur_state;	/* current state of search */
	ring_data_t *		proc_ring;	/* processed objects */
	ring_data_t *		unproc_ring;	/* unprocessed objects */
	ring_data_t *		bg_ops;	/* unprocessed objects */
	unsigned long		bg_status;
	void *		bg_froot;
} search_context_t;

/*
 * These are the prototypes of the device operations that
 * in the file ls_device.c
 */
extern int device_stop(device_state_t *dev, int id);
extern int device_terminate(device_state_t *dev, int id);
extern int device_start(device_state_t *dev, int id);
extern int device_set_searchlet(device_state_t *dev, int id, char *filter,
	                        char *spec);
extern device_state_t * device_init(search_context_t *sc, int id);

/*
 * These are the function prototypes for the device emulation
 * function in dev_emul.c
 */
extern int dev_emul_term(device_state_t *dev);
extern int dev_emul_init(device_state_t *dev);
extern int dev_emul_next_obj(obj_data_t **new_obj, device_state_t *dev);

/*
 * These are background processing functions.
 */
extern int bg_init(search_context_t *sc, int id);
extern int bg_set_searchlet(search_context_t *sc, int id, char *filter_name,
			char *spec_name);

#endif
