
#ifndef _FILTER_EXEC_H_
#define _FILTER_EXEC_H_


struct filter_info_t;

extern int init_filters(char*filter_file, char *filter_spce,  
			struct filter_info_t **finfo);

extern int eval_filters(obj_data_t *obj_handle, struct filter_info_t *finfo);



#endif /* _FILTER_EXEC_H_ */
