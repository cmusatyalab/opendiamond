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

#define	MAX_DEV_GROUPS		8

typedef struct device_handle {
	struct device_handle * 	next;
	uint32_t		dev_id;	
	group_id_t		dev_groups[MAX_DEV_GROUPS];
	int			num_groups;
	unsigned int		flags;
	void *			dev_handle;
	int			ver_no;
	struct search_context *	sc;
} device_handle_t;


#define	MAX_DEV_PER_GROUP	32
typedef struct gid_map {
	struct gid_map *	next;
	group_id_t		gid;
	int			num_dev;
	uint32_t		devs[MAX_DEV_PER_GROUP];
} gid_map_t;


typedef enum {
	SS_ACTIVE,		/* a search is currently in progress */
	SS_DONE,		/* search active, all object are processed */
	SS_EMPTY,
	SS_SHUTDOWN,	
	SS_IDLE	
} search_status_t;


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
struct filter_info;
typedef struct search_context {
	int			cur_search_id;	/* ID of current search */
	device_handle_t *	dev_list;
	search_status_t		cur_status;	/* current status of search */
	ring_data_t *		proc_ring;	/* processed objects */
	ring_data_t *		unproc_ring;	/* unprocessed objects */
	ring_data_t *		bg_ops;	/* unprocessed objects */
	ring_data_t *		log_ring;	/* data to log */
	unsigned long		bg_status;
	struct filter_info     *bg_froot; /* filter_info_t -RW */
} search_context_t;

/*
 * These are the prototypes of the device operations that
 * in the file ls_device.c
 */
extern int dev_new_obj_cb(void *hcookie, obj_data_t *odata, int vno);
extern void dev_log_data_cb(void *cookie, char *data, int len, int devid);
extern int lookup_group_hosts(group_id_t gid, int *num_hosts, 
		uint32_t *hostids);
extern int device_add_gid(search_context_t *sc, group_id_t gid, uint32_t devid);
/*
 * These are background processing functions.
 */
extern int bg_init(search_context_t *sc, int id);
extern int bg_set_searchlet(search_context_t *sc, int id, char *filter_name,
			char *spec_name);


extern int log_start(search_context_t *sc);


extern int dctl_start(search_context_t *sc);

gid_map_t *read_gid_map(char *mapfile);

#endif
