
#ifndef _FILTER_EXEC_H_
#define _FILTER_EXEC_H_


struct filter_info;

extern int init_filters(char*filter_file, char *filter_spce,  
			struct filter_info **finfo);

extern int eval_filters(obj_data_t *obj_handle, struct filter_info *finfo);
extern int fexec_num_filters(struct filter_info *finfo);
extern int fexec_clear_stats(struct filter_info *finfo);
extern int fexec_get_stats(struct filter_info *finfo, int max, 
		filter_stats_t *fstats);



#endif /* _FILTER_EXEC_H_ */
