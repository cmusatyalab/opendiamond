#ifndef _FILTER_EXEC_H_
#define _FILTER_EXEC_H_ 1

#ifdef __cplusplus
extern "C" {
#endif

struct filter_data;
typedef struct filter_data filter_data_t;


/* 
 * optimizer policy setup
 */
enum policy_type_t {
  NULL_POLICY=0,
  HILL_CLIMB_POLICY,
  BEST_FIRST_POLICY,
  INDEP_POLICY,
  RANDOM_POLICY,
  STATIC_POLICY
};

struct filter_exec_t {
  enum policy_type_t current_policy;
};
/* update at your own risk! */
extern struct filter_exec_t filter_exec;


/* 
 * functions
 */

void    fexec_system_init();

int     fexec_load_searchlet(char* filterfile, char *fspec, 
				filter_data_t **fdata);
int     fexec_init_search(filter_data_t *fdata);
int     fexec_term_search(filter_data_t *fdata);
int     eval_filters(obj_data_t *obj_handle, filter_data_t *fdata,
            int force_eval, void *cookie,
		     int (*cb_func)(void *cookie, char *name, int *pass, uint64_t* et));
int     fexec_num_filters(filter_data_t *fdata);
void    fexec_clear_stats( filter_data_t *fdata);
double  fexec_get_load(filter_data_t *fdata);
int     fexec_set_blob( filter_data_t *fdata, char *filter_name, 
				int blob_len, void *blob_data);
int     fexec_get_stats(filter_data_t *fdata , int max, filter_stats_t *fstats);
char *  fexec_cur_filtname();

#ifdef __cplusplus
}
#endif

#endif /* _FILTER_EXEC_H_ */
