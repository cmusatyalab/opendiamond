%option nounput
%{
/*
 * 	Diamond (Release 1.0)
 *      A system for interactive brute-force search
 *
 *      Copyright (c) 2002-2005, Intel Corporation
 *      All Rights Reserved
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#include <pthread.h>
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

static int  have_error();
static int  have_data_dir(char *name);

static char * eat_token(char *str);
static void do_init();

typedef struct	dconfig_info {
	char *	dconf_name;
	char *	dconf_datadir;
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
DATADIR[ \t]+[[:alnum:]_\/]+ 	{have_data_dir(eat_token(yytext));}
.				{ have_error(); }
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
have_data_dir(char *dir)
{
	dinfo->dconf_datadir = strdup(dir);
	printf("data dir %s \n", dinfo->dconf_datadir); 	
	return(0);
}

char *
dconf_get_datadir()
{
	char *dir;
	do_init();
	dir = strdup(dinfo->dconf_datadir);
	return(dir);
}


#define	MAX_PATH				256
#define DIAMOND_CONFIG_ENV_NAME			"DIAMOND_CONFIG"
#define DIAMOND_CONFIG_FILE_NAME		"diamond_config"
#define DIAMOND_DEFAULT_PATH			"/usr/local/diamond/etc"

/*
 * This looks for the config file in a couple of places.
 * the first place is in $DIAMOND_CONFIG.  Next it looks in
 * the local directory, and third it looks in the system defined
 * location.
 */

static FILE *
open_config_file()
{
	FILE *  new_file;
	char *  lkup;
	char	fname[MAX_PATH];


	/* try the environment variable */
	lkup = getenv(DIAMOND_CONFIG_ENV_NAME);
	if (lkup != NULL) {
		/* XXX deal with overflow */
		snprintf(fname, MAX_PATH, "%s", lkup);
		fname[MAX_PATH - 1] = '\0';
		new_file = fopen(fname, "r");
		if (new_file != NULL) {
            	dinfo->dconf_name = strdup(fname);
            	assert(dinfo->dconf_name != NULL);
				return(new_file);
		}
	}

		
	/* try the local directory */
	new_file = fopen(DIAMOND_CONFIG_FILE_NAME, "r");
	if (new_file != NULL) {
        dinfo->dconf_name = strdup(DIAMOND_CONFIG_FILE_NAME);
        assert(dinfo->dconf_name != NULL);
		return(new_file);
	}

	/* try the default absolute path */
	/* XXX deal with overflow */
	snprintf(fname, MAX_PATH, "%s/%s", DIAMOND_DEFAULT_PATH,
		DIAMOND_CONFIG_FILE_NAME);
	fname[MAX_PATH - 1] = '\0';
	new_file = fopen(fname, "r");
	if (new_file != NULL) {
        dinfo->dconf_name = strdup(fname);
        assert(dinfo->dconf_name != NULL);
		return(new_file);
	}

	/* didn't find a file, return NULL */
	return(NULL);
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

	if (dinfo != NULL) {
		return;	
	}

	dinfo = malloc(sizeof(*dinfo));
	assert(dinfo != NULL);

	yyin = open_config_file();
	if (yyin == NULL) {
		/* XXX log */
		fprintf(stderr, "failed to open diamond config file \n");
		exit(1);
	}

	ret = dconflex();
	if (ret) {
		/* printf("error from yylex\n"); */
        free(dinfo->dconf_name);
		free(dinfo);
		dinfo = NULL;
		exit(1);
	}

	return;
}
