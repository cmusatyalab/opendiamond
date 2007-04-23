%option nounput
%option prefix="dconf"
%{
/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 2
 *
 *  Copyright (c) 2002-2005 Intel Corporation
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>
#include <ctype.h>
#include <stdint.h>
#include <netdb.h>
#include <assert.h>
#include "diamond_consts.h"
#include "diamond_types.h"
#include "lib_dconfig.h"
#include "dconfig_priv.h"

static int  have_error();
static int  set_data_root(char *name);
static int  set_cache_dir(char *name);
static int  set_index_dir(char *name);
static int  set_log_dir(char *name);
static int  set_data_type(data_type_t type);

static char * eat_token(char *str);
static void do_init();

typedef struct	dconfig_info {
	char *		dconf_name;
	char *		dconf_dataroot;
	char *		dconf_cachedir;
	char *		dconf_indexdir;
	char *      dconf_logdir;
	data_type_t	dconf_dtype;
} dconf_info_t;


static int err = 0;
static dconf_info_t *dinfo = NULL;

%}
%pointer


HEX8	[[:xdigit:]][[:xdigit:]]
HEX16	{HEX8}:{HEX8}
HEX32	{HEX16}:{HEX16}
HEX64	{HEX32}:{HEX32}
NAME	[a-zA-Z][a-zA-Z0-9_-]*
                                                                                
FLOAT (([0-9]*"."[0-9]+)|([0-9]+))
FNAME (([0-9]*"."[0-9]+)|([0-9]+))
                                                                                
%%
#.*\n
DATAROOT[ \t]+[[:alnum:]\-_\/]+	{set_data_root(eat_token(yytext));}
CACHEDIR[ \t]+[[:alnum:]\-_\/]+	{set_cache_dir(eat_token(yytext));}
INDEXDIR[ \t]+[[:alnum:]\-_\/]+	{set_index_dir(eat_token(yytext));}
LOGDIR[ \t]+[[:alnum:]\-_\/]+	{set_log_dir(eat_token(yytext));}
DATATYPE[ \t]+OBJECT	 	{set_data_type(DATA_TYPE_OBJECT);}
DATATYPE[ \t]+NATIVE	 	{set_data_type(DATA_TYPE_NATIVE);}
.				{have_error(); }
%%


                                                                                
static char *
eat_token(char *str)
{
        char * cur = str;
                                                                                
        while (!isspace(*cur)) {
                cur++;
        }
        while (isspace(*cur)) {
                cur++;
        }
        return(cur);
}

int
have_error()
{
	fprintf(stderr, "unknown text %s\n", yytext);
	err = 1;
	return (1);
}





/*
 * We have a new names.  We allocate a structure to keep track
 * of it.
 */
 

static int
set_data_root(char *dir)
{
	dinfo->dconf_dataroot = strdup(dir);
	return(0);
}

char *
dconf_get_dataroot()
{
	char *dir;
	do_init();
	dir = strdup(dinfo->dconf_dataroot);
	return(dir);
}

static int
set_cache_dir(char *dir)
{
	dinfo->dconf_cachedir = strdup(dir);
	return(0);
}

char *
dconf_get_cachedir()
{
	char *dir;
	do_init();
	dir = strdup(dinfo->dconf_cachedir);
	return(dir);
}


char *
dconf_get_binary_cachedir()
{
	char *dir;
	do_init();

	dir = malloc(PATH_MAX);

	snprintf(dir, PATH_MAX, "%s/%s", dinfo->dconf_cachedir, 
	    BINARY_DIAMOND_CACHE);
	return(dir);
}

char *
dconf_get_filter_cachedir()
{
	char *dir;
	do_init();

	dir = malloc(PATH_MAX);

	snprintf(dir, PATH_MAX, "%s/%s", dinfo->dconf_cachedir, 
	    FILTER_DIAMOND_CACHE);
	return(dir);
}

char *
dconf_get_blob_cachedir()
{
	char *dir;
	do_init();

	dir = malloc(PATH_MAX);

	snprintf(dir, PATH_MAX, "%s/%s", dinfo->dconf_cachedir, 
	    BLOB_DIAMOND_CACHE);
	return(dir);
}


char *
dconf_get_spec_cachedir()
{
	char *dir;
	do_init();
	dir = malloc(PATH_MAX);
	snprintf(dir, PATH_MAX, "%s/%s", dinfo->dconf_cachedir, 
	    SPEC_DIAMOND_CACHE);
	return(dir);
}

static int
set_index_dir(char *dir)
{
	dinfo->dconf_indexdir = strdup(dir);
	return(0);
}

static int
set_log_dir(char *dir)
{
	dinfo->dconf_logdir = strdup(dir);
	return(0);
}

char *
dconf_get_indexdir()
{
	char *dir;
	do_init();
	dir = strdup(dinfo->dconf_indexdir);
	return(dir);
}

char *
dconf_get_logdir()
{
	char *dir;
	do_init();
	dir = strdup(dinfo->dconf_logdir);
	return(dir);
}

static int
set_data_type(data_type_t type)
{
	dinfo->dconf_dtype = type;
	return(0);
}

data_type_t
dconf_get_datatype()
{
	do_init();
	return(dinfo->dconf_dtype);
}

static void
setup_dir(const char *dir)
{
	int	err;
	err = mkdir(dir, 0700);
	if (err && (errno != EEXIST)) {
		fprintf(stderr, "Failed to initialize dir %s \n", dir);
	}
	return(dir);
}



int
dconfwrap()
{
    return(1);
}

static void
do_init()
{
	int ret;
	char *dir;

	if (dinfo != NULL) {
		return;	
	}

	dinfo = malloc(sizeof(*dinfo));
	assert(dinfo != NULL);

	/* setup defaults */
	/* XXX other defaults */
	dinfo->dconf_cachedir = DEFAULT_DIAMOND_CACHE;
	dinfo->dconf_logdir = DEFAULT_DIAMOND_LOG;
	
	yyin = dconfig_open_config_file(DIAMOND_CONFIG_FILE_NAME);
	if (yyin == NULL) {
		/* XXX log */
		fprintf(stderr, "failed to open diamond config file \n");
		exit(1);
	}

	ret = dconflex();
	if (ret) {
        	// XXX free(dinfo->dconf_name);
		free(dinfo);
		dinfo = NULL;
		exit(1);
	}

	/* setup all the directories we need if they don't exist */
	dir = dconf_get_cachedir();
	setup_dir(dir);
	free(dir);
	
	dir = dconf_get_logdir();
	setup_dir(dir);
	free(dir);

	dir = dconf_get_spec_cachedir();
	setup_dir(dir);
	free(dir);

	dir = dconf_get_binary_cachedir();
	setup_dir(dir);
	free(dir);

	dir = dconf_get_filter_cachedir();
	setup_dir(dir);
	free(dir);

	dir = dconf_get_blob_cachedir();
	setup_dir(dir);
	free(dir);

	return;
}
