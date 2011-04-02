%option nounput noyywrap reentrant
%option prefix="fspec"
%{
/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 4
 *
 *  Copyright (c) 2002-2005 Intel Corporation
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

// stop flex from printing garbage to stdout
#define ECHO

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <ctype.h>
#include "sig_calc.h"
#include "sys_attr.h"
#include "diamond_consts.h"
#include "diamond_types.h"
#include "lib_odisk.h"
#include "lib_filter.h"
#include "lib_filterexec.h"
#include "filter_priv.h"
#include "string_helpers.h"
#include <assert.h>

static void new_filter(yyscan_t scanner);
static void filter_threshold(yyscan_t scanner);
static void filter_merit(yyscan_t scanner);
static void filter_threshhold(void);
static void filter_args(yyscan_t scanner);
static void filter_eval_fn(yyscan_t scanner);
static void filter_init_fn(yyscan_t scanner);
static void filter_fini_fn(yyscan_t scanner);
static void filter_signature(yyscan_t scanner);
static void filter_dependency(yyscan_t scanner);
static void filter_in_obj(yyscan_t scanner);
static void filter_out_obj(filter_output_type_t type, yyscan_t scanner);
static int  filter_error(yyscan_t scanner);

/* #define VERBOSE */

#define         DATA_INCR           16    

struct filter_spec_lex {
  int cur_filt_id;
  filter_data_t *fdata;
};

%}
ID      [a-zA-Z][-a-zA-Z0-9_]*
%%
#.*\n
ARG[ \t]+[[:graph:]]+			        {filter_args(yyscanner); }
FILTER[ \t]+{ID}			            {new_filter(yyscanner); }
THRESHOLD[ \t]+[0-9]+ 			        {filter_threshold(yyscanner); }
THRESHHOLD[ \t]+[0-9]+ 			        {filter_threshhold(); }
EVAL_FUNCTION[ \t]+[[:alnum:]_]+ 		{filter_eval_fn(yyscanner); }
INIT_FUNCTION[ \t]+[[:alnum:]_]+ 		{filter_init_fn(yyscanner); }
FINI_FUNCTION[ \t]+[[:alnum:]_]+ 		{filter_fini_fn(yyscanner); }
SIGNATURE[ \t]+[0-9a-f]+			{filter_signature(yyscanner); }
REQUIRES[ \t]+{ID}			            {filter_dependency(yyscanner); }
IN_OBJECT[ \t]+[0-9]+			        {filter_in_obj(yyscanner); }
OUT_OBJECT[ \t]+UNMODIFIED[ \t]+[0-9]+	{filter_out_obj(FO_UNMODIFIED, yyscanner); }
OUT_OBJECT[ \t]+NEW[ \t]+[0-9]+		    {filter_out_obj(FO_NEW, yyscanner); }
OUT_OBJECT[ \t]+CLONE[ \t]+[0-9]+	    {filter_out_obj(FO_CLONE, yyscanner); }
OUT_OBJECT[ \t]+COPY_ATTR[ \t]+[0-9]+	{filter_out_obj(FO_COPY_ATTR, yyscanner); }
MERIT[ \t]+[0-9]+			            {filter_merit(yyscanner); }
[[:blank:]]
\n
.					{ filter_error(yyscanner); }
%%


/*
 * This is a common function that is used to remove
 * the first token and following whitespace.
 */

static char *
eat_token(char *str)
{
	char * cur = str;

	while ((isalpha(*cur))||(*cur == '_')) {
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
new_filter(yyscan_t scanner)
{
    char * name;
    char ** arg_data;
    struct filter_spec_lex *extra =
      (struct filter_spec_lex *) yyget_extra(scanner);

    name = eat_token(yyget_text(scanner));
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
    if (extra->fdata->fd_num_filters == extra->fdata->fd_max_filters) {
        int     new_size;
        int     new_num;

        new_num = extra->fdata->fd_max_filters + DATA_INCR;
        new_size = sizeof(*extra->fdata) + (new_num * sizeof(filter_info_t));

        extra->fdata = realloc(extra->fdata, new_size);
        assert(extra->fdata != NULL);
        extra->fdata->fd_max_filters = new_num;
    }
  
#ifdef VERBOSE
    printf("fdata %p num_filt %d max %d \n", extra->fdata,
	   extra->fdata->fd_num_filters,
	   extra->fdata->fd_max_filters);
#endif

    extra->cur_filt_id = extra->fdata->fd_num_filters;
    extra->fdata->fd_num_filters++;

    memset(&extra->fdata->fd_filters[extra->cur_filt_id], 0, sizeof(filter_info_t));

    strcpy(extra->fdata->fd_filters[extra->cur_filt_id].fi_name, name);

#ifdef VERBOSE
    printf(" id %d name %s \n", extra->cur_filt_id, extra->fdata->fd_filters[extra->cur_filt_id].fi_name);
#endif
    extra->fdata->fd_filters[extra->cur_filt_id].fi_threshold = -1;
    extra->fdata->fd_filters[extra->cur_filt_id].fi_merit = 0;
    extra->fdata->fd_filters[extra->cur_filt_id].fi_filterid = extra->cur_filt_id;

    extra->fdata->fd_filters[extra->cur_filt_id].fi_numargs = 0;

	arg_data = (char **)malloc(sizeof(char *) * ARG_CHUNK);
	assert(arg_data != NULL);
    extra->fdata->fd_filters[extra->cur_filt_id].fi_maxargs = ARG_CHUNK;
    extra->fdata->fd_filters[extra->cur_filt_id].fi_arglist = arg_data;

    extra->fdata->fd_filters[extra->cur_filt_id].fi_added_bytes = 0;
   
    if (strcmp(name, "APPLICATION") == 0) {
        extra->fdata->fd_app_id = extra->cur_filt_id;
    }
    /* XXX save application name */
}


int 
filter_error(yyscan_t scanner)
{
	fprintf(stderr, "unknown text %s\n", yyget_text(scanner));
	assert(0);
	exit(1);
}

static void
filter_threshhold(void)
{

	fprintf(stderr, "please rename THRESHHOLD -> THRESHOLD\n");
	exit(1);

}

static void
filter_threshold(yyscan_t scanner)
{
    char *th_str;
    th_str = eat_token(yyget_text(scanner));
    struct filter_spec_lex *extra =
      (struct filter_spec_lex *) yyget_extra(scanner);

    if (extra->cur_filt_id == -1) {
		fprintf(stderr, "parse error: no filter active\n");
		assert(0);
		return;
    }
    
	extra->fdata->fd_filters[extra->cur_filt_id].fi_threshold = atoi(th_str);

#ifdef VERBOSE
	printf("filter threshold <%d> \n", cur_filter->fi_threshold);
#endif

}



static void
filter_merit(yyscan_t scanner)
{
    char *val_str;
    val_str = eat_token(yyget_text(scanner));
    struct filter_spec_lex *extra =
      (struct filter_spec_lex *) yyget_extra(scanner);

    if (extra->cur_filt_id == -1) {
		fprintf(stderr, "parse error: no filter active\n");
		assert(0);
		return;
    }
    
	extra->fdata->fd_filters[extra->cur_filt_id].fi_merit = atoi(val_str);

#ifdef VERBOSE
	printf("filter merit <%d> \n", cur_filter->fi_merit);
#endif

}

static void
filter_args(yyscan_t scanner)
{
    char *          arg_str;
    char *          arg_data;
    int             len;
    filter_info_t*  this_filter;

    arg_str = eat_token(yyget_text(scanner)); 

    struct filter_spec_lex *extra =
      (struct filter_spec_lex *) yyget_extra(scanner);

    if (extra->cur_filt_id == -1) {
		fprintf(stderr, "parse error: no filter active\n");
		assert(0);
		return;
    }

	this_filter = &extra->fdata->fd_filters[extra->cur_filt_id];
    

	len = strlen(arg_str) + 1;
	arg_data = (char *)malloc(len);
	if (arg_data == NULL) {
		fprintf(stderr, "malloc: can't allocate data\n");
		return;
	}

	strcpy(arg_data, arg_str);
#ifdef VERBOSE
	printf("arg %s\n", arg_data);
#endif

	if (this_filter->fi_numargs >= this_filter->fi_maxargs) {
		int			new_size;
		char **		arg_list;

		this_filter->fi_maxargs += ARG_CHUNK;
		new_size = sizeof(char *) * this_filter->fi_maxargs;
		arg_list = realloc(this_filter->fi_arglist, new_size);
		assert(arg_list != NULL);

		
		this_filter->fi_arglist = arg_list;	
	}

	this_filter->fi_arglist[this_filter->fi_numargs] = arg_data;
	this_filter->fi_numargs++;
}



static void
filter_eval_fn(yyscan_t scanner)
{
    char *fname;

    fname = eat_token(yyget_text(scanner));
    if (strlen(fname) >= MAX_FILTER_FUNC_NAME) {
      /* XXX error */
      return;
    }

    struct filter_spec_lex *extra =
      (struct filter_spec_lex *) yyget_extra(scanner);

    if (extra->cur_filt_id == -1) {
		fprintf(stderr, "parse error: no filter active\n");
		assert(0);
		return;
    }

	strcpy(extra->fdata->fd_filters[extra->cur_filt_id].fi_eval_name, fname);

}

static void
filter_init_fn(yyscan_t scanner)
{
	char *fname;
	struct filter_spec_lex *extra =
	  (struct filter_spec_lex *) yyget_extra(scanner);

	fname = eat_token(yyget_text(scanner));
	if (strlen(fname) >= MAX_FILTER_FUNC_NAME) {
		/* XXX error */
		return;
	}

    if (extra->cur_filt_id == -1) {
		fprintf(stderr, "parse error: no filter active\n");
		assert(0);
		return;
    }

	strcpy(extra->fdata->fd_filters[extra->cur_filt_id].fi_init_name, fname);

}

static void
filter_fini_fn(yyscan_t scanner)
{
	char *fname;
	struct filter_spec_lex *extra =
	  (struct filter_spec_lex *) yyget_extra(scanner);

	fname = eat_token(yyget_text(scanner));
	if (strlen(fname) >= MAX_FILTER_FUNC_NAME) {
		/* XXX error */
		return;
	}

    if (extra->cur_filt_id == -1) {
		fprintf(stderr, "parse error: no filter active\n");
		assert(0);
		return;
    }

	strcpy(extra->fdata->fd_filters[extra->cur_filt_id].fi_fini_name, fname);
}


static void
filter_signature(yyscan_t scanner)
{
	GString *str;
	struct filter_spec_lex *extra =
	  (struct filter_spec_lex *) yyget_extra(scanner);

	str = g_string_new(eat_token(yyget_text(scanner)));
	string_hex_decode(str);
	if (str->len != SIG_SIZE) {
		/* XXX error */
		goto out;
	}

	if (extra->cur_filt_id == -1) {
		fprintf(stderr, "parse error: no filter active\n");
		assert(0);
		goto out;
	}

	memcpy(extra->fdata->fd_filters[extra->cur_filt_id].fi_signature,
	       str->str, str->len);
out:
	g_string_free(str, TRUE);
}


static void
filter_dependency(yyscan_t scanner)
{
	char *arg;
	filter_info_t * this_filter;

	struct filter_spec_lex *extra =
	  (struct filter_spec_lex *) yyget_extra(scanner);

	arg = eat_token(yyget_text(scanner));

    if (extra->cur_filt_id == -1) {
		fprintf(stderr, "parse error: no filter active\n");
		assert(0);
		return;
    }

    this_filter = &extra->fdata->fd_filters[extra->cur_filt_id];

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
filter_in_obj(yyscan_t scanner)
{
	char *arg;
	struct filter_spec_lex *extra =
	  (struct filter_spec_lex *) yyget_extra(scanner);

	arg = eat_token(yyget_text(scanner));

    if (extra->cur_filt_id == -1) {
		fprintf(stderr, "parse error: no filter active\n");
		assert(0);
		return;
    }

    extra->fdata->fd_filters[extra->cur_filt_id].fi_blocksize_in = atol(arg);

#ifdef VERBOSE
	printf("input blocksize %d\n", 
        extra->fdata->fd_filters[extra->cur_filt_id].fi_blocksize_in);
#endif
}


static void
filter_out_obj(filter_output_type_t type, yyscan_t scanner)
{
	char *arg;
	struct filter_spec_lex *extra =
	  (struct filter_spec_lex *) yyget_extra(scanner);

	arg = eat_token(yyget_text(scanner));

    if (extra->cur_filt_id == -1) {
		fprintf(stderr, "parse error: no filter active\n");
		assert(0);
		return;
    }

    extra->fdata->fd_filters[extra->cur_filt_id].fi_type_out = type;

	arg = eat_token(arg); 
    extra->fdata->fd_filters[extra->cur_filt_id].fi_blocksize_out = atoi(arg);


#ifdef VERBOSE
	printf("output blocksize %d\n", 
        extra->fdata->fd_filters[extra->cur_filt_id].fi_blocksize_out);
#endif
}


int
read_filter_spec(char *spec_name, filter_data_t **fdp)
{
    yyscan_t scanner;
    struct filter_spec_lex extra;

	/* initialize static variables */
	/* first_filter = NULL; */
	/* cur_filter = NULL; */

    extra.cur_filt_id = -1;

    /* allocate initial storage for the filter data */
    extra.fdata = (filter_data_t *)malloc(sizeof(*extra.fdata) + ((DATA_INCR) * 
            sizeof(filter_info_t)));
    assert(extra.fdata != NULL); 

    extra.fdata->fd_max_filters = DATA_INCR;
    extra.fdata->fd_num_filters = 0;
    extra.fdata->fd_perm = NULL;
    extra.fdata->fd_po = NULL;

    extra.fdata->obj_counter = 0;
    extra.fdata->fd_avg_wall = 1.0;
    extra.fdata->fd_avg_exec = 1.0;
    extra.fdata->full_eval = 0;

    extra.fdata->code_info = malloc(sizeof(fcode_info_t) * FCODE_INCREMENT);
    assert(extra.fdata->code_info != NULL);
    extra.fdata->max_codes = FCODE_INCREMENT;
    extra.fdata->num_codes = 0;


    FILE *f = fopen(spec_name, "r");
    assert(f != NULL);

    yylex_init(&scanner);
    yyset_in(f, scanner);
    yyset_extra(&extra, scanner);
    yylex(scanner);
    yylex_destroy(scanner);

    fclose(f);

    *fdp = extra.fdata;

    return(0);
}
