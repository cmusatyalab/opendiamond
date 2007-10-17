%option nounput noyywrap reentrant
%option prefix="glkup"
%{
/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 3
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
#include "gid_map.h"
#include "dconfig_priv.h"

static void have_gid(yyscan_t scanner);
static void have_cont(yyscan_t scanner);
static int have_name(yyscan_t scanner);
static void have_newline(yyscan_t scanner);
static int  have_error(yyscan_t scanner);


struct group_lookup_lex {
  gid_map_t *groot;
  int cont;
  int err;
};


%}
HEX8	[[:xdigit:]][[:xdigit:]]
HEX16	{HEX8}:{HEX8}
HEX32	{HEX16}:{HEX16}
HEX64	{HEX32}:{HEX32}
NAME	[a-zA-Z0-9][a-zA-Z0-9.-]*[[:blank:]]*

%%
#.*\n
{HEX64}[[:blank:]]*{NAME}*		{ have_gid(yyscanner); }
{NAME}*					{ have_name(yyscanner); }
[[:blank:]]
\\					{ have_cont(yyscanner); }
.					{ have_error(yyscanner); }

\n					{ have_newline(yyscanner); }
%%



static char *
eat_gid(char *str)
{
	char * cur = str;

	while ((isxdigit(*cur)) || (*cur == ':')) {
		cur++;
	}


	while ((isspace(*cur)) && (*cur != '\0')) {
		cur++;
	}

	if (*cur == '\0') {
		return(NULL);
	} else  {
		return(cur);
	}
}


static char *
eat_name(char **str)
{
	char * cur = *str;
	char * name = *str;

	/* first get past the current name */
	while ((*cur != '\t') && (*cur != ' ') && (*cur != '\0')) {
		cur++;
	}

	/* put null here for the current name */
	if (*cur == '\0') {
		*str = NULL;
		return(name);
	}	

	*cur = '\0';
	cur++;

	/* now get rid of any space before the next item */
	while ((isspace(*cur)) && (*cur != '\0')) {
		cur++;
	}

	if (*cur == '\0') {
		*str = NULL;
	} else  {
		*str = cur;
	}

	return(name);
}




int have_error(yyscan_t scanner)
{
	struct group_lookup_lex *extra =
	  (struct group_lookup_lex *) yyget_extra(scanner);

	fprintf(stderr, "unknown text %s\n", yyget_text(scanner));
	extra->err = 1;
	return (1);
}

static void 
have_newline(yyscan_t scanner)
{
	struct group_lookup_lex *extra =
	  (struct group_lookup_lex *) yyget_extra(scanner);

	if (extra->cont == 2) {
		extra->cont = 1;
	} else { 
		extra->cont = 0;
	}
}



static groupid_t
convert_gid(char *str)
{
	groupid_t	gid;
	int		i;
	long int	newval;
	char		buf[4];

	gid = 0;

	for (i=0; i<8; i++) {	
		buf[0] = str[0];	
		buf[1] = str[1];	
		buf[2] = '\0';

		newval = strtol(buf, NULL, 16);
		gid = (gid * 256) + newval;

		str += 3;
	}

	return(gid);
}

static uint32_t
lookup_devname(char *name)
{
	struct hostent *hent;
	uint32_t	addr;

	hent = gethostbyname(name);
	if (hent == NULL) {
        printf("Unable to resolve device %s \n", name);
        return(0);
	}

	/*
	 * If we have more than one item, the 
	 * use the first.
	 */

	assert(hent->h_length == 4);

	addr = *((uint32_t *)hent->h_addr_list[0]);

	return(addr);
}

static void
have_gid(yyscan_t scanner)
{
	char *		names;
	char *		cur_name;
	uint32_t	dev_id;

	gid_map_t *	gmap;

	struct group_lookup_lex *extra =
	  (struct group_lookup_lex *) yyget_extra(scanner);

	gmap = (gid_map_t *)malloc(sizeof(*gmap));
	if (gmap == NULL) {
		/* XXX log */
		extra->err = 1;
		return;
	}

	gmap->num_dev = 0;
	gmap->next = extra->groot;
	extra->groot = gmap;

		

	gmap->gid = convert_gid(yyget_text(scanner));

	names = eat_gid(yyget_text(scanner));

	while (names != NULL) {
		cur_name = eat_name(&names);
		dev_id = lookup_devname(cur_name);

		if (gmap->num_dev >= MAX_DEV_PER_GROUP) {
			/* XXX log */
			extra->err = 1;
			return;
		}

		gmap->devs[gmap->num_dev].dev_name = strdup(cur_name);
		gmap->devs[gmap->num_dev].dev_id = dev_id;
		gmap->num_dev++;
	}

	extra->cont = 0;

	return;
}

static void
have_cont(yyscan_t scanner)
{
	struct group_lookup_lex *extra =
	  (struct group_lookup_lex *) yyget_extra(scanner);

	extra->cont = 2;
	return;
}


 
static int
have_name(yyscan_t scanner)
{
	char *		names;
	char *		cur_name;
	uint32_t	dev_id;
	gid_map_t *	gmap;

	struct group_lookup_lex *extra =
	  (struct group_lookup_lex *) yyget_extra(scanner);

	if (extra->cont != 1) {
		/* XXX log */
		extra->err = 1;
		return(1);
	}


	gmap = extra->groot;
	names = yyget_text(scanner);

	while (names != NULL) {
		cur_name = eat_name(&names);
		dev_id = lookup_devname(cur_name);

		if (gmap->num_dev >= MAX_DEV_PER_GROUP) {
			/* XXX log */
			extra->err = 1;
			return(1);
		}
		gmap->devs[gmap->num_dev].dev_name = strdup(cur_name);
		gmap->devs[gmap->num_dev].dev_id = dev_id;
		gmap->num_dev++;
	}

	extra->cont = 0;

	return(0);
}


static void
free_gid_map(gid_map_t *map) {
	gid_map_t *trav, *next;

	for(trav = map; trav != NULL; trav = next) {
		 next = trav->next;
		 free(trav);
	}
}

static void
read_gid_map(char *map_file, yyscan_t scanner)
{
	int ret;
	FILE *f;

	struct group_lookup_lex *extra =
	  (struct group_lookup_lex *) yyget_extra(scanner);

	f = dconfig_open_config_file(map_file);
	if (f == NULL) {
		/* XXX log */
		fprintf(stderr, "failed to open gid_map file ");
		return;
	}

	yyset_in(f, scanner);
	ret = yylex(scanner);

	fclose(f);

	if (ret) {
		fprintf(stderr, "error from yylex\n");
		return;
	}


	if (extra->err) {
		fprintf(stderr, "error from yylex\n");
		free_gid_map(extra->groot);
		extra->groot = NULL;
		return;
	}

	return;
}

int
glkup_gid_hosts(groupid_t gid, int *num_hosts, uint32_t *hostids)
{
	gid_map_t	*cur_map;
	int		 i;

	yyscan_t scanner;
	struct group_lookup_lex extra = {
	  .cont = 0,
	  .err = 0,
	  .groot = NULL,
	};

	yylex_init(&scanner);
	yyset_extra(&extra, scanner);
	read_gid_map("gid_map", scanner);
	yylex_destroy(scanner);

	cur_map = extra.groot;

	if (cur_map == NULL) {
		fprintf(stderr, "Unable to read gid_map \n");
		*num_hosts = 0;
		return(ENOENT);
	}

	for(; cur_map != NULL; cur_map = cur_map->next)
		if (cur_map->gid == gid)
			break;

	if (cur_map == NULL) {
		*num_hosts = 0;
		free_gid_map(extra.groot);
		return(ENOENT);
	}


	if (cur_map->num_dev > *num_hosts) {
		/* XXX log */
		*num_hosts = cur_map->num_dev;
		free_gid_map(extra.groot);
		return(ENOMEM);
	}

	for (i = 0; i < cur_map->num_dev; i++) {
	        if (cur_map->devs[i].dev_id == 0) {
		    fprintf(stderr, "unable to resolve %s \n", 
			    cur_map->devs[i].dev_name);
		    free_gid_map(extra.groot);
	            return(ENOENT);
	        }
		hostids[i] = cur_map->devs[i].dev_id;
	}
	*num_hosts = cur_map->num_dev;

	free_gid_map(extra.groot);
	extra.groot = NULL;

	return(0);
}
