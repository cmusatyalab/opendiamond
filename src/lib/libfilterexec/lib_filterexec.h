#ifndef _FILTER_EXEC_H_
#define _FILTER_EXEC_H_ 1


struct filter_data;
typedef struct filter_data filter_data_t;

int     init_filters(char* filterfile, char *fspec, filter_data_t **fdata);
int     eval_filters(obj_data_t *obj_handle, filter_data_t *fdata);
int     fexec_num_filters(filter_data_t *fdata);
void    fexec_clear_stats( filter_data_t *fdata);
int     fexec_get_stats(filter_data_t *fdata , int max, filter_stats_t *fstats);
char *  fexec_cur_filtname();



#endif /* _FILTER_EXEC_H_ */
