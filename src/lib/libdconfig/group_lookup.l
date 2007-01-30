%option nounput
%option prefix="glkup"
%{
/*
 * 	Diamond
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

/*
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
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
#include "gid_map.h"
#include "dconfig_priv.h"

static void have_gid();
static void have_cont();
static int have_name();
static void have_newline();
static int  have_error();


static int cont = 0;
static int err = 0;
static gid_map_t 	*groot = NULL;

%}
%array
HEX8	[[:xdigit:]][[:xdigit:]]
HEX16	{HEX8}:{HEX8}
HEX32	{HEX16}:{HEX16}
HEX64	{HEX32}:{HEX32}
NAME	[a-zA-Z0-9][a-zA-Z0-9.-]*[[:blank:]]*

%%
#.*\n
{HEX64}[[:blank:]]*{NAME}*		{ have_gid(); }
{NAME}*					{ have_name(); }
[[:blank:]]
\\					{ have_cont(); }
.					{ have_error(); }

\n					{ have_newline(); }
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




int have_error()
{
	fprintf(stderr, "unknown text %s\n", yytext);
	err = 1;
	return (1);
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
have_gid()
{
	char *		names;
	char *		cur_name;
	uint32_t	dev_id;

	gid_map_t *	gmap;

	gmap = (gid_map_t *)malloc(sizeof(*gmap));
	if (gmap == NULL) {
		/* XXX log */
		err = 1;
		return;
	}

	gmap->num_dev = 0;
	gmap->next = groot;
	groot = gmap;

		

	gmap->gid = convert_gid(yytext);

	names = eat_gid(yytext);

	while (names != NULL) {
		cur_name = eat_name(&names);
		dev_id = lookup_devname(cur_name);

		if (gmap->num_dev >= MAX_DEV_PER_GROUP) {
			/* XXX log */
			err = 1;
			return;
		}

		gmap->devs[gmap->num_dev].dev_name = strdup(cur_name);
		gmap->devs[gmap->num_dev].dev_id = dev_id;
		gmap->num_dev++;
	}

	cont = 0;

	return;
}

static void
have_cont()
{
	cont = 2;
	return;
}


 
static int
have_name()
{
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

	return(0);
}


int
glkupwrap()
{
	return(1);
}



int
glkup_gid_hosts(groupid_t gid, int *num_hosts, uint32_t *hostids)
{
	static gid_map_t *	gid_map = NULL;
	gid_map_t *		cur_map;
	int			i;


	if (gid_map == NULL) {
		/* XXX */
		gid_map = read_gid_map("gid_map");
	}

	if (gid_map == NULL) {
        fprintf(stderr, "Unable to find gid_map \n");
        assert(0);

		*num_hosts = 0;
		return(ENOENT);
	}


	cur_map = gid_map;
	while (cur_map != NULL) {
		if (cur_map->gid == gid) {
			break;
		}

		cur_map = cur_map->next;
	}


	if (cur_map == NULL) {
		*num_hosts = 0;
		return(ENOENT);
	}


	if (cur_map->num_dev > *num_hosts) {
		/* XXX log */
		*num_hosts = cur_map->num_dev;
		return(ENOMEM);
	}

	for (i = 0; i < cur_map->num_dev; i++) {
        if (cur_map->devs[i].dev_id == 0) {
            fprintf(stderr, "unable to resolve %s \n", 
                cur_map->devs[i].dev_name);
            return(ENOENT);
        }
		hostids[i] = cur_map->devs[i].dev_id;
	}
	*num_hosts = cur_map->num_dev;
	return(0);
}



gid_map_t *
read_gid_map(char *map_file)
{
	int ret;

	printf("reading gid map (%s)...\n", map_file);
	yyin = dconfig_open_config_file(map_file);
	if (yyin == NULL) {
		/* XXX log */
		fprintf(stderr, "failed to open map file ");
		exit(1);
	}
	ret = yylex();
	if (ret) {
		/* printf("error from yylex\n"); */
		exit(1);
	}


	if (err) {
		/* XXX clean up list */
		return(NULL);
	}	

	return(groot);
	return(0);
}
