%option nounput noyywrap reentrant
%option prefix="dconf"
%{
/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 4
 *
 *  Copyright (c) 2002-2005 Intel Corporation
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
 *  Copyright (c) 2009 Carnegie Mellon University
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

// stop flex from printing garbage to stdout
#define ECHO

#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
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
#include <limits.h>
#include <glib.h>
#include "diamond_consts.h"
#include "diamond_types.h"
#include "dconfig_priv.h"

static int  have_error(yyscan_t scanner);
static int  set_cache_dir(const char *name);
static int  set_log_dir(const char *name);
static int  set_certfile(const char *name);
static int  set_serverid(const char *name);
static int  set_data_type(data_type_t type);

static char * eat_token(char *str);
static void do_init(void);

typedef struct	dconfig_info {
	char *		dconf_cachedir;
	char *		dconf_logdir;
	char *		dconf_certfile;
	GPtrArray *	dconf_serverids;
	data_type_t	dconf_dtype;
} dconf_info_t;


static dconf_info_t *dinfo = NULL;

%}


HEX8	[[:xdigit:]][[:xdigit:]]
HEX16	{HEX8}:{HEX8}
HEX32	{HEX16}:{HEX16}
HEX64	{HEX32}:{HEX32}
NAME	[a-zA-Z][a-zA-Z0-9_-]*
                                                                                
FLOAT (([0-9]*"."[0-9]+)|([0-9]+))
FNAME (([0-9]*"."[0-9]+)|([0-9]+))
                                                                                
%%
#.*\n
DATAROOT[ \t]+.*\n
INDEXDIR[ \t]+.*\n
CACHEDIR[ \t]+[[:alnum:]\-_\/\.]+ {set_cache_dir(eat_token(yyget_text(yyscanner)));}
LOGDIR[ \t]+[[:alnum:]\-_\/\.]+	  {set_log_dir(eat_token(yyget_text(yyscanner)));}
CERTFILE[ \t]+[[:alnum:]\-_\/\.]+ {set_certfile(eat_token(yyget_text(yyscanner)));}
SERVERID[ \t]+[[:alnum:]\-_\/\.]+ {set_serverid(eat_token(yyget_text(yyscanner)));}
DATATYPE[ \t]+OBJECT	 	{set_data_type(DATA_TYPE_OBJECT);}
DATATYPE[ \t]+NATIVE	 	{set_data_type(DATA_TYPE_NATIVE);}
.				{have_error(yyscanner); }
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
have_error(yyscan_t scanner)
{
	fprintf(stderr, "unknown text %s\n", yyget_text(scanner));
	return (1);
}





/*
 * We have a new names.  We allocate a structure to keep track
 * of it.
 */
 

static int
set_cache_dir(const char *dir)
{
	dinfo->dconf_cachedir = strdup(dir);
	return(0);
}

char *
dconf_get_cachedir(void)
{
	char *dir;
	do_init();
	dir = strdup(dinfo->dconf_cachedir);
	return(dir);
}

static char *concatpath (const char *prefix, const char *suffix)
{
	char *dir = malloc(PATH_MAX);
	snprintf(dir, PATH_MAX, "%s/%s", prefix, suffix);
	return(dir);
}


char *
dconf_get_binary_cachedir(void)
{
	do_init();
	return(concatpath(dinfo->dconf_cachedir, BINARY_DIAMOND_CACHE));
}

char *
dconf_get_filter_cachedir(void)
{
	do_init();
	return(concatpath(dinfo->dconf_cachedir, FILTER_DIAMOND_CACHE));
}


char *
dconf_get_blob_cachedir(void)
{
	do_init();
	return(concatpath(dinfo->dconf_cachedir, BLOB_DIAMOND_CACHE));
}


char *
dconf_get_spec_cachedir(void)
{
	do_init();
	return(concatpath(dinfo->dconf_cachedir, SPEC_DIAMOND_CACHE));
}

static int
set_log_dir(const char *dir)
{
	dinfo->dconf_logdir = strdup(dir);
	return(0);
}

char *
dconf_get_logdir(void)
{
	char *dir;
	do_init();
	dir = strdup(dinfo->dconf_logdir);
	return(dir);
}

static int
set_certfile(const char *certfile)
{
	dinfo->dconf_certfile = strdup(certfile);
	return(0);
}

char *
dconf_get_certfile(void)
{
	do_init();
	if (!dinfo->dconf_certfile) return NULL;
	return strdup(dinfo->dconf_certfile);
}

static int
set_serverid(const char *name)
{
	g_ptr_array_add(dinfo->dconf_serverids, strdup(name));
	return(0);
}

char **
dconf_get_serverids(void)
{
	do_init();
	return (char **)dinfo->dconf_serverids->pdata;
}

static int
set_data_type(data_type_t type)
{
	dinfo->dconf_dtype = type;
	return(0);
}

data_type_t
dconf_get_datatype(void)
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
}


static void
do_init(void)
{
	int ret;
	char *tmp, name[NI_MAXHOST];
	struct addrinfo *p, *ai = NULL;
	yyscan_t scanner;
	FILE *f;

	/* use a mutex to ensure lexer is re-entrant */
	static pthread_mutex_t lexlock = PTHREAD_MUTEX_INITIALIZER;

	assert(pthread_mutex_lock(&lexlock) == 0);

	if (dinfo != NULL) {
		pthread_mutex_unlock(&lexlock);
		return;	
	}

	dinfo = calloc(1, sizeof(*dinfo));
	assert(dinfo != NULL);

	/* setup defaults */
	/* XXX other defaults */
	dinfo->dconf_cachedir = DEFAULT_DIAMOND_CACHE;
	dinfo->dconf_logdir = DEFAULT_DIAMOND_LOG;
	dinfo->dconf_serverids = g_ptr_array_new();


	f = dconfig_open_config_file(DIAMOND_CONFIG_FILE_NAME);
	if (f != NULL) {
		// if f is NULL, then just use the defaults
		yylex_init(&scanner);
		yyset_in(f, scanner);
		ret = yylex(scanner);
		yylex_destroy(scanner);

		fclose(f);

		if (ret) {
		  free(dinfo);
		  dinfo = NULL;
		  exit(1);
		}
	}

	/* if no hostnames were specified, find all of our fqdns */
	if (dinfo->dconf_serverids->len == 0) {
	    gethostname(name, sizeof(name));
	    getaddrinfo(name, NULL, NULL, &ai);
	    for (p = ai; p; p = p->ai_next) {
		if (getnameinfo(p->ai_addr, p->ai_addrlen, name, sizeof(name),
				NULL, 0, NI_NAMEREQD))
		    continue;
		g_ptr_array_add(dinfo->dconf_serverids, name);
	    }
	    freeaddrinfo(ai);
	}
	g_ptr_array_add(dinfo->dconf_serverids, NULL);

	/* setup all the directories we need if they don't exist */
	setup_dir(dinfo->dconf_logdir);

	setup_dir(dinfo->dconf_cachedir);

	tmp = concatpath(dinfo->dconf_cachedir, SPEC_DIAMOND_CACHE);
	setup_dir(tmp);
	free(tmp);

	tmp = concatpath(dinfo->dconf_cachedir, BINARY_DIAMOND_CACHE);
	setup_dir(tmp);
	free(tmp);

	tmp = concatpath(dinfo->dconf_cachedir, FILTER_DIAMOND_CACHE);
	setup_dir(tmp);
	free(tmp);

	tmp = concatpath(dinfo->dconf_cachedir, BLOB_DIAMOND_CACHE);
	setup_dir(tmp);
	free(tmp);

	pthread_mutex_unlock(&lexlock);
	return;
}
