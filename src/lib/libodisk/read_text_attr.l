%option nounput
%option prefix="glkup"
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
#include "lib_odisk.h"
#include "odisk_priv.h"
#include "obj_attr.h"


static void have_value();
static void have_cont();
static void have_error();
static void have_newline();
static void have_indirect_value();


static int cont = 0;
static int err = 0;

%}
%array
HEX8	[[:xdigit:]][[:xdigit:]]
HEX16	{HEX8}:{HEX8}
HEX32	{HEX16}:{HEX16}
HEX64	{HEX32}:{HEX32}
NAME	[a-zA-Z0-9._][a-zA-Z0-9._]*[[:blank:]]*

%%
#.*\n
{NAME}={NAME}				{ have_value(); }
\\					{ have_cont(); }
.					{ have_error(); }
\n					{ have_newline(); }
%%






static void 
have_error()
{
	fprintf(stderr, "unknown text %s\n", yytext);
	err = 1;
}

static void 
have_newline()
{
	if (cont == 2) {
		cont = 1;
	} else { 
		cont = 0;
	}
}





static void
have_cont()
{
	cont = 2;
	return;
}


static void
have_value()
{
#ifdef	XXX
	char *		names;
	char *		cur_name;
	uint32_t	dev_id;
	gid_map_t *	gmap;


	if (cont != 1) {
		/* XXX log */
		err = 1;
		return(1);
	}


	gmap = groot;
	names = yytext;

	while (names != NULL) {
		cur_name = eat_name(&names);
		dev_id = lookup_devname(cur_name);

		if (gmap->num_dev >= MAX_DEV_PER_GROUP) {
			/* XXX log */
			err = 1;
			return(1);
		}
		gmap->devs[gmap->num_dev].dev_name = strdup(cur_name);
		gmap->devs[gmap->num_dev].dev_id = dev_id;
		gmap->num_dev++;
	}

	cont = 0;
#endif
}


int
glkupwrap()
{
	return(1);
}

static FILE *
open_map_file(char *aname)
{
	FILE *new_file;
	/* try the local directory */
	new_file = fopen(aname, "r");
	if (new_file != NULL) {
		/* printf("XXX local %s \n", map_name); */
		return(new_file);
	}	

	return(NULL);

}



void
obj_load_text_attr(odisk_state_t *odisk, char *attr_name, obj_data_t *new_obj)
{
	int ret;

	yyin = dconfig_open_config_file(attr_name);

	/* if no file then exit */
	if (yyin == NULL) {
		return;
	}
#ifdef	XXX
	ret = yylex();
	if (ret) {
		/* printf("error from yylex\n"); */
		exit(1);
	}


	if (err) {
		/* XXX clean up list */
		return(NULL);
	}	

#endif
	fclose(yyin);

}
