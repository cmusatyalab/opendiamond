%{
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <ctype.h>
#include "ring.h"
#include "attr.h"
#include "lib_od.h"
#include "lib_odisk.h"
#include "lib_filter.h"
#include "lib_searchlet.h"
#include "filter_exec.h"
#include "filter_priv.h"
#include <assert.h>

static void new_filter();
static void filter_threshold();
static void filter_merit();
static void filter_threshhold();
static void filter_args();
static void filter_functionname();
static void filter_dependency();
static void filter_in_obj();
static void filter_out_obj();
static int  filter_error();

/* #define VERBOSE */

#define         DATA_INCR           16    
static int cur_filt_id = -1;

filter_data_t   *fdata;

%}
%array
ID      [a-zA-Z][-a-zA-Z0-9_]*
%%
#.*\n
ARG[ \t]+[[:graph:]]+			        {filter_args(); }
FILTER[ \t]+{ID}			            {new_filter(); }
THRESHOLD[ \t]+[0-9]+ 			        {filter_threshold(); }
THRESHHOLD[ \t]+[0-9]+ 			        {filter_threshhold(); }
FUNCTION[ \t]+[[:alnum:]_]+		        {filter_functionname(); }
REQUIRES[ \t]+{ID}			            {filter_dependency(); }
IN_OBJECT[ \t]+[0-9]+			        {filter_in_obj(); }
OUT_OBJECT[ \t]+UNMODIFIED[ \t]+[0-9]+	{filter_out_obj(FO_UNMODIFIED); }
OUT_OBJECT[ \t]+NEW[ \t]+[0-9]+		    {filter_out_obj(FO_NEW); }
OUT_OBJECT[ \t]+CLONE[ \t]+[0-9]+	    {filter_out_obj(FO_CLONE); }
OUT_OBJECT[ \t]+COPY_ATTR[ \t]+[0-9]+	{filter_out_obj(FO_COPY_ATTR); }
MERIT[ \t]+[0-9]+			            {filter_merit(); }
[[:blank:]]
\n
.					{ filter_error(); }
%%


/*
 * This is a common function that is used to remove
 * the first token and following whitespace.
 */

static char *
eat_token(char *str)
{
	char * cur = str;

	while (isalpha(*cur)) {
		cur++;
	}
	while (isspace(*cur)) {
		cur++;
	}
	return(cur);
}


/*
 * Create  a new filter of the type "name"
 */

static void
new_filter()
{
	char * name;
	name = eat_token(yytext); 
#ifdef VERBOSE
	printf("filter name <%s> \n", name);
#endif
	if (strlen(name) >= MAX_FILTER_NAME) {
		/* XXX error */
		return;
	}

 
    /*
     * Make sure we have enough space for this filters, otherwise,
     * allocate some more space.
     */
    if (fdata->fd_num_filters == fdata->fd_max_filters) {
        int     new_size;
        int     new_num;

        new_num = fdata->fd_max_filters + DATA_INCR;
        new_size = sizeof(*fdata) + (new_num * sizeof(filter_info_t));

        fdata = realloc(fdata, new_size);
        assert(fdata != NULL);
        fdata->fd_max_filters = new_num;
    }
  
    printf("fdata %p num_filt %d max %d \n", fdata, fdata->fd_num_filters,
	   fdata->fd_max_filters);
    cur_filt_id = fdata->fd_num_filters;
    fdata->fd_num_filters++;

    memset(&fdata->fd_filters[cur_filt_id], 0, sizeof(filter_info_t));

    strcpy(fdata->fd_filters[cur_filt_id].fi_name, name);
    printf(" id %d name %s \n", cur_filt_id, fdata->fd_filters[cur_filt_id].fi_name);
    fdata->fd_filters[cur_filt_id].fi_threshold = -1;
    fdata->fd_filters[cur_filt_id].fi_merit = 0;
    fdata->fd_filters[cur_filt_id].fi_numargs = 0;
    fdata->fd_filters[cur_filt_id].fi_filterid = cur_filt_id;
    
    if (strcmp(name, "APPLICATION") == 0) {
        fdata->fd_app_id = cur_filt_id;
    }
    /* XXX save application name */
}


int 
filter_error()
{
	fprintf(stderr, "unknown text %s\n", yytext);
	assert(0);
	exit(1);
}

static void
filter_threshhold()
{

	fprintf(stderr, "please rename THRESHHOLD -> THRESHOLD\n");
	exit(1);

}

static void
filter_threshold()
{
	char *th_str;
	th_str = eat_token(yytext); 

    if (cur_filt_id == -1) {
		fprintf(stderr, "parse error: no filter active\n");
		assert(0);
		return;
    }
    
	fdata->fd_filters[cur_filt_id].fi_threshold = atoi(th_str);

#ifdef VERBOSE
	printf("filter threshold <%d> \n", cur_filter->fi_threshold);
#endif

}



static void
filter_merit()
{
	char *val_str;
	val_str = eat_token(yytext); 

    if (cur_filt_id == -1) {
		fprintf(stderr, "parse error: no filter active\n");
		assert(0);
		return;
    }
    
	fdata->fd_filters[cur_filt_id].fi_merit = atoi(val_str);

#ifdef VERBOSE
	printf("filter merit <%d> \n", cur_filter->fi_merit);
#endif

}

static void
filter_args()
{
	char *          arg_str;
	char *          arg_data;
	int             len;
    filter_info_t*  this_filter;

	arg_str = eat_token(yytext); 

    if (cur_filt_id == -1) {
		fprintf(stderr, "parse error: no filter active\n");
		assert(0);
		return;
    }

    this_filter = &fdata->fd_filters[cur_filt_id];
    
	if ((this_filter->fi_numargs + 1) >= MAX_NUM_ARGS) {
		fprintf(stderr, "Too many args \n");
		return;
	}

	len = strlen(arg_str);
	arg_data = (char *)malloc(len+2);
	if (arg_data == NULL) {
		fprintf(stderr, "malloc: can't allocate data\n");
		return;
	}

	strcpy(arg_data, arg_str);
#ifdef VERBOSE
	printf("arg %s\n", arg_data);
#endif

	this_filter->fi_args[this_filter->fi_numargs] = arg_data;
	this_filter->fi_numargs++;
}



static void
filter_functionname()
{
	char *fname;

	fname = eat_token(yytext); 
	if (strlen(fname) >= MAX_FILTER_FUNC_NAME) {
		/* XXX error */
		return;
	}

    if (cur_filt_id == -1) {
		fprintf(stderr, "parse error: no filter active\n");
		assert(0);
		return;
    }

	strcpy(fdata->fd_filters[cur_filt_id].fi_fname, fname);

#ifdef VERBOSE
	printf("function name %s \n", fname);
#endif

}

static void
filter_dependency()
{
	char *arg;
    filter_info_t * this_filter;

	arg = eat_token(yytext); 

    if (cur_filt_id == -1) {
		fprintf(stderr, "parse error: no filter active\n");
		assert(0);
		return;
    }

    this_filter = &fdata->fd_filters[cur_filt_id];

	if (this_filter->fi_depcount >= MAX_NUM_DEPS) {
		/* XXX error */
		return;
	}

	this_filter->fi_deps[this_filter->fi_depcount++].name = strdup(arg);

#ifdef VERBOSE
	printf("dependency %s\n", arg);
#endif
}


static void
filter_in_obj()
{
	char *arg;

	arg = eat_token(yytext); 

    if (cur_filt_id == -1) {
		fprintf(stderr, "parse error: no filter active\n");
		assert(0);
		return;
    }

    fdata->fd_filters[cur_filt_id].fi_blocksize_in = atol(arg);

#ifdef VERBOSE
	printf("input blocksize %d\n", 
        fdata->fd_filters[cur_filt_id].fi_blocksize_in);
#endif
}


static void
filter_out_obj(filter_output_type_t type)
{
	char *arg;

	arg = eat_token(yytext); 

    if (cur_filt_id == -1) {
		fprintf(stderr, "parse error: no filter active\n");
		assert(0);
		return;
    }

    fdata->fd_filters[cur_filt_id].fi_type_out = type;

	arg = eat_token(arg); 
    fdata->fd_filters[cur_filt_id].fi_blocksize_out = atoi(arg);


#ifdef VERBOSE
	printf("output blocksize %d\n", 
        fdata->fd_filters[cur_filt_id].fi_blocksize_out);
#endif
}


int
fspecwrap()
{
	return(1);
}


int
read_filter_spec(char *spec_name, filter_data_t **fdp)
{
    int i;
	int err;

	/* initialize static variables */
	/* first_filter = NULL; */
	/* cur_filter = NULL; */

    cur_filt_id = -1;

    /* allocate initial storage for the filter data */
    fdata = (filter_data_t *)malloc(sizeof(*fdata) + ((DATA_INCR) * 
            sizeof(filter_info_t)));
    assert(fdata != NULL); 

    fdata->fd_max_filters = DATA_INCR;
    fdata->fd_num_filters = 0;
    fdata->fd_perm = NULL;
    fdata->fd_po = NULL;

    for (i=0; i < PROB_HASH_BUCKETS; i++) {
        LIST_INIT(&fdata->fd_prob_hash[i]);    
    }

	yyin = fopen(spec_name, "r");
    assert(yyin != NULL);
	err = fspeclex();

    *fdp = fdata;

	return(0);
}

