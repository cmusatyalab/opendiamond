%{
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>
#include <ctype.h>
#include "ring.h"
#include "attr.h"
#include "lib_odisk.h"
#include "lib_filter.h"
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

static char *root_filter = "APPLICATION";
static filter_info_t *first_filter = NULL;
static filter_info_t *cur_filter = NULL;
%}
%array
ID      [a-zA-Z][a-zA-Z0-9_]*
%%
#.*\n
ARG[ \t]+[[:graph:]]+			{filter_args(); }
FILTER[ \t]+{ID}			{new_filter(); }
THRESHOLD[ \t]+[0-9]+ 			{filter_threshold(); }
THRESHHOLD[ \t]+[0-9]+ 			{filter_threshhold(); }
FUNCTION[ \t]+[[:alnum:]_]+		{filter_functionname(); }
REQUIRES[ \t]+{ID}			{filter_dependency(); }
IN_OBJECT[ \t]+[0-9]+			{filter_in_obj(); }
OUT_OBJECT[ \t]+UNMODIFIED[ \t]+[0-9]+	{filter_out_obj(FO_UNMODIFIED); }
OUT_OBJECT[ \t]+NEW[ \t]+[0-9]+		{filter_out_obj(FO_NEW); }
OUT_OBJECT[ \t]+CLONE[ \t]+[0-9]+	{filter_out_obj(FO_CLONE); }
OUT_OBJECT[ \t]+COPY_ATTR[ \t]+[0-9]+	{filter_out_obj(FO_COPY_ATTR); }
MERIT[ \t]+[0-9]+			{filter_merit(); }
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
	char *name;
	filter_info_t	*new_filt;
	name = eat_token(yytext); 
#ifdef VERBOSE
	printf("filter name <%s> \n", name);
#endif
	if (strlen(name) >= MAX_FILTER_NAME) {
		/* XXX error */
		return;
	}
	new_filt = (filter_info_t *)malloc(sizeof(*new_filt));
	if (new_filter == NULL) {
		/* XXX error log */
		return;
	}

	memset(new_filt, 0, sizeof(*new_filt));
	strcpy(new_filt->fi_name, name);
	new_filt->fi_threshold = -1;
	new_filt->fi_merit = 0;
	new_filt->fi_numargs = 0;

	if (first_filter == NULL) {
		first_filter = new_filt;
	} else {
		cur_filter->fi_next = new_filt;
	}
	cur_filter = new_filt;

}

int filter_error()
{
	fprintf(stderr, "unknown text %s\n", yytext);
	return (0);
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

	if (cur_filter == NULL) {
		fprintf(stderr, "parse error: no filter active\n");
		assert(0);
		return;
	}

	cur_filter->fi_threshold = atoi(th_str);
#ifdef VERBOSE
	printf("filter threshold <%d> \n", cur_filter->fi_threshold);
#endif

}



static void
filter_merit()
{
	char *val_str;
	val_str = eat_token(yytext); 

	if (cur_filter == NULL) {
		fprintf(stderr, "parse error: no filter active\n");
		assert(0);
		return;
	}

	cur_filter->fi_merit = atoi(val_str);
#ifdef VERBOSE
	printf("filter merit <%d> \n", cur_filter->fi_merit);
#endif

}

static void
filter_args()
{
	char *arg_str;
	char *arg_data;
	int   len;

	arg_str = eat_token(yytext); 

	if (cur_filter == NULL) {
		fprintf(stderr, "parse error: no filter active\n");
		assert(0);
		return;
	}

	if ((cur_filter->fi_numargs + 1) >= MAX_NUM_ARGS) {
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

	cur_filter->fi_args[cur_filter->fi_numargs] = arg_data;
	cur_filter->fi_numargs++;
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

	if (cur_filter == NULL) {
		fprintf(stderr, "parse error: no filter active\n");
		return;
	}

	strcpy(cur_filter->fi_fname, fname);
#ifdef VERBOSE
	printf("function name %s \n", cur_filter->fi_fname);
#endif

}

static void
filter_dependency()
{
	char *arg;

	arg = eat_token(yytext); 

	if (cur_filter == NULL) {
		fprintf(stderr, "parse error: no filter active\n");
		return;
	}

	if(cur_filter->fi_depcount >= MAX_NUM_DEPS) {
		/* XXX error */
		return;
	}

	cur_filter->fi_deps[cur_filter->fi_depcount++].name = strdup(arg);

#ifdef VERBOSE
	printf("dependency %s\n", arg);
#endif
}


static void
filter_in_obj()
{
	char *arg;

	arg = eat_token(yytext); 

	if (cur_filter == NULL) {
		fprintf(stderr, "parse error: no filter active\n");
		return;
	}

	cur_filter->fi_blocksize_in = atol(arg);

#ifdef VERBOSE
	printf("input blocksize %d\n", cur_filter->fi_blocksize_in);
#endif
}


static void
filter_out_obj(filter_output_type_t type)
{
	char *arg;

	arg = eat_token(yytext); 

	if (cur_filter == NULL) {
		fprintf(stderr, "parse error: no filter active\n");
		return;
	}

	cur_filter->fi_type_out = type;
	arg = eat_token(arg); 

	cur_filter->fi_blocksize_out = atol(arg);

#ifdef VERBOSE
	printf("output blocksize %d\n", cur_filter->fi_blocksize_out);
#endif
}


int
fspecwrap()
{
	return(1);
}


int
read_filter_spec(char *spec_name, filter_info_t **froot, char **troot)
{

	int err;

	/* initialize static variables */
	root_filter = "APPLICATION";
	first_filter = NULL;
	cur_filter = NULL;

	printf("reading filter spec (%s)...\n", spec_name);
	yyin = fopen(spec_name, "r");
	err = fspeclex();

	*froot = first_filter;
	*troot = root_filter;

	return(0);
}
