#ifndef	_LIB_OCACHE_H_
#define	_LIB_OCACHE_H_ 	1

//#include "lib_od.h"
//#include "lib_odisk.h"
//#include "filter_exec.h"
//#include "obj_attr.h"
//#include "lib_filter.h"

#ifdef	__cplusplus
extern "C" {
#endif

#define ATTR_ENTRY_NUM  50

struct ocache_state;

typedef struct {
	unsigned int  name_len;
	char *attr_name;
	char attr_sig[16];
} cache_attr_entry;

typedef struct {
	unsigned int entry_num;
	cache_attr_entry **entry_data;
} cache_attr_set;

struct cache_obj_s {
	uint64_t                oid;
	char			filter_sig[16];
	char			*file_name;
	int			result;
	int			eval_count; //how many times this filter is evaluated
	cache_attr_set		iattr;
	cache_attr_set		oattr;
	struct cache_obj_s	*next;
};

typedef void (*stats_drop)(void *cookie);
typedef void (*stats_process)(void *cookie);

struct ceval_state;

typedef struct ceval_state {
        pthread_t       ceval_thread_id;   // thread for cache table
        filter_data_t * fdata;
        odisk_state_t * odisk;
        void * cookie;
        stats_drop stats_drop_fn;
        stats_drop stats_process_fn;
} ceval_state_t;

typedef struct cache_obj_s cache_obj;

#define		INSERT_START	0
#define		INSERT_IATTR	1
#define		INSERT_OATTR	2
#define		INSERT_END	3

typedef struct {
	char			filter_sig[16]; 
	char			*file_name;  /*cached oattr file name*/
} cache_start_entry;

typedef struct {
	int			type;
	uint64_t                oid;
	union {
		cache_start_entry	start;
		cache_attr_entry	iattr;		/*add input attr*/
		cache_attr_entry	oattr;		/*add output attr*/
		int			result;		/*end*/
	} u;
} cache_ring_entry;

typedef struct {
	unsigned int name_len;
	char *name;
	off_t data_len;
	char *data;
} cache_attr_t;

typedef struct {
	int			type;
	uint64_t                oid;
	union {
		char                    *file_name;     /* the file name to cache oattr */
		cache_attr_t		oattr;		/*add output attr*/
	} u;
} oattr_ring_entry;

int digest_cal(char *lib_name, char *filt_name, int numarg, char **filt_args, int blob_len, void *blob, unsigned char ** signature);
int ocache_lookup(uint64_t local_id, char *fsig, cache_attr_set *change_attr, int *err, cache_attr_set **oattr_set, char **fpath);
int ocache_lookup2(uint64_t local_id, char *fsig, cache_attr_set *change_attr, int *conf, cache_attr_set **oattr_set, char **fpath, int flag);
int ocache_wait_lookup(obj_data_t *lobj, char *fsig, cache_attr_set *change_attr, cache_attr_set **oattr_set);
int ocache_init(char *path_name, void *dctl_cookie, void * log_cookie);
int ocache_start();
int ocache_stop(char *path_name);
int ocache_wait_finish();
int ocache_read_file(char *name);
int sig_cal(void *buf, off_t buflen, unsigned char **signature);
int ocache_add_start(char *fhandle, uint64_t obj_id, unsigned char *fsig, int lookup, char *fpath);
int ocache_add_end(char *fhandle, uint64_t obj_id, int conf);
int combine_attr_set(cache_attr_set *attr1, cache_attr_set *attr2);

int ceval_init_search(filter_data_t * fdata, struct ceval_state *cstate);

int ceval_init(struct ceval_state **cstate, odisk_state_t *odisk, void *cookie, stats_drop stats_drop_fn, stats_process stats_process_fn);

int ceval_start();
int ceval_stop();

int ceval_filters1(uint64_t oid, filter_data_t * fdata, void *cookie, 
                         int (*cb_func) (void *cookie, char *name,
                                           int *pass, uint64_t * et));
int ceval_filters2(obj_data_t * obj_handle, filter_data_t * fdata, int force_eval,
             void *cookie, int (*continue_cb)(void *cookie),
                         int (*cb_func) (void *cookie, char *name,
                                           int *pass, uint64_t * et));


#ifdef	__cplusplus
}
#endif

#endif	/* !_LIB_OCACHE_H */

