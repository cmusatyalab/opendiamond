

/* evaluate a permutation */
int fexec_evaluate(filter_data_t *fdata, permutation_t *perm, int gen, int *utility);
int fexec_evaluate_indep(filter_data_t *fdata, permutation_t *perm, int gen, int *utility);

/* evaluate a single filter */
int fexec_single(filter_data_t *fdata, int fid, int *utility);


/* debug */
void fexec_print_cost(const filter_data_t *fdata, const permutation_t *perm);

void fstat_add_obj_info(filter_data_t *fdata, int pass, rtime_t time_ns);
char *fstat_sprint(char *buf, const filter_data_t *fdata);
