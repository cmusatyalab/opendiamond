/*
 *
 *
 *                          Diamond 1.0
 * 
 *            Copyright (c) 2002-2004, Intel Corporation
 *                         All Rights Reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 *    * Neither the name of Intel nor the names of its contributors may
 *      be used to endorse or promote products derived from this software 
 *      without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _FILTER_EXEC_H_
#define _FILTER_EXEC_H_ 1

#ifdef __cplusplus
extern          "C" {
#endif

    struct filter_data;
    typedef struct filter_data filter_data_t;


    /*
     * optimizer policy setup
     */
    enum policy_type_t {
        NULL_POLICY = 0,
        HILL_CLIMB_POLICY,
        BEST_FIRST_POLICY,
        INDEP_POLICY,
        RANDOM_POLICY,
        STATIC_POLICY
    };

    enum bypass_type_t {
        BP_NONE = 0,
        BP_SIMPLE,
        BP_GREEDY,
        BP_HYBRID
    };

	enum auto_part_t {
        AUTO_PART_NONE = 0,
        AUTO_PART_BYPASS,
        AUTO_PART_QUEUE
    };

    extern int             fexec_bypass_type;
    extern int             fexec_autopart_type;

    struct filter_exec_t {
        enum policy_type_t current_policy;
    };
    /*
     * update at your own risk! 
     */
    extern struct filter_exec_t filter_exec;


    /*
     * functions
     */

    void            fexec_system_init();

    int             fexec_load_searchlet(char *filterfile, char *fspec,
                                         filter_data_t ** fdata);
    int             fexec_init_search(filter_data_t * fdata);
    int             fexec_term_search(filter_data_t * fdata);
    int             eval_filters(obj_data_t * obj_handle,
                                 filter_data_t * fdata, int force_eval,
                                 void *cookie, int (*continue_cb)(void* vookie),
								int (*cb_func) (void *cookie, char *name,
                                                               int *pass,
                                                               uint64_t *
                                                               et));
    int             fexec_num_filters(filter_data_t * fdata);
    void            fexec_clear_stats(filter_data_t * fdata);
    double          fexec_get_load(filter_data_t * fdata);
    int             fexec_set_blob(filter_data_t * fdata, char *filter_name,
                                   int blob_len, void *blob_data);
    int             fexec_get_stats(filter_data_t * fdata, int max,
                                    filter_stats_t * fstats);
    char           *fexec_cur_filtname();



    int             fexec_update_bypass(filter_data_t * fdata, double ratio);
    int             fexec_update_grouping(filter_data_t * fdata, double ratio);

#ifdef __cplusplus
}
#endif
#endif                          /* _FILTER_EXEC_H_ */
