

/* evaluate a permutation */
int fexec_evaluate(filter_data_t *fdata, permutation_t *perm, int gen, int *utility);

/* evaluate a single filter */
int fexec_single(filter_data_t *fdata, int fid, int *utility);


/* debug */
void fexec_print_cost(const filter_data_t *fdata, const permutation_t *perm);
