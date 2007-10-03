%option nounput
%option prefix="nlkup"
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
#include "name_map.h"
#include "lib_dconfig.h"
#include "dconfig_priv.h"

static void have_gid();
static void have_cont();
static int have_name();
static void have_newline();
static int  have_error();


static int cont = 0;
static int err = 0;
static name_map_t 	*nroot = NULL;
static name_info_t  ninfo;

%}
%pointer
HEX8	[[:xdigit:]][[:xdigit:]]
HEX16	{HEX8}:{HEX8}
HEX32	{HEX16}:{HEX16}
HEX64	{HEX32}:{HEX32}
NAME	[a-zA-Z][a-zA-Z0-9_-]*

%%
#.*\n
{HEX64}                         { have_gid(); }
{NAME}*					        { have_name(); }
[[:blank:]]
\\					            { have_cont(); }
.					            { have_error(); }
\n					            { have_newline(); }
%%


int
have_error()
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

	/* printf("%0llx \n", gid); */

	return(gid);
}


static void
have_gid()
{
    groupid_t   gid;

    if (cont != 1) {
        fprintf(stderr, "invalid format \n");
        err = 1;
        return;
    }
	
    gid = convert_gid(yytext);

    assert(nroot->num_gids < MAX_GROUP_PER_NAME); 

    /* this entry belongs to the one on the head of the list */
    nroot->gids[nroot->num_gids] = gid;
    nroot->num_gids++;

	return;
}

static void
have_cont()
{
	cont = 2;
	return;
}


/*
 * We have a new names.  We allocate a structure to keep track
 * of it.
 */
 
static int
have_name()
{
	name_map_t *	nmap;


	if (cont != 0) {
        fprintf(stderr, "invalid format \n");
		/* XXX log */
		err = 1;
		return(1);
	}


    /*
     * Allocate the entry to put it on the list.
     */
	nmap = (name_map_t *)malloc(sizeof(*nmap));
	if (nmap == NULL) {
		/* XXX log */
		err = 1;
		return(0);
	}

	nmap->num_gids = 0;
    nmap->name = strdup(yytext);
    assert(nmap->name != NULL);

    /* put this on the list */
	nmap->next = nroot;
	nroot = nmap;

    /*
     * set the continuation to let use know that GID's are valid
     * to accept.
     */
	cont = 1;

	return(0);
}

/*
 * Return a list of GID's associated with the specified symbolic name.
 */
int
nlkup_lookup_collection(char *name, int *num_gids, groupid_t *gids)
{
	name_map_t *        cur_map, *tmp_map = NULL;
	int			i;

	if (nroot != NULL) {   /* XXX if accessed concurrently with
				* nlkup_*_entry by two or more threads. */
		tmp_map = nroot;
	        nroot = read_name_map("name_map");
	}
	else if (nroot == NULL)
		nroot = read_name_map("name_map");
	
	if (nroot == NULL) {
	        fprintf(stderr, "Unable to find name_map \n");
	        *num_gids = 0;
		if(tmp_map) nroot = tmp_map;
	        return(ENOENT);
	}


	for (cur_map = nroot; cur_map != NULL; cur_map = cur_map->next)
	        if (strcmp(name, cur_map->name) == 0)
			break;

	if (cur_map == NULL) {
		*num_gids = 0;
		if(tmp_map) nroot = tmp_map;
		return(ENOENT);
	}


	if (cur_map->num_gids > *num_gids) {
		/* XXX log */
		*num_gids = cur_map->num_gids;
		if(tmp_map) nroot = tmp_map;
		return(ENOMEM);
	}

	for (i = 0; i < cur_map->num_gids; i++) {
		gids[i] = cur_map->gids[i];
	}
	*num_gids = cur_map->num_gids;

	if(tmp_map) 
		nroot = tmp_map;

	return(0);
}

int
nlkup_first_entry(char **namep, void **cookiep)
{
    if(nroot != NULL) {
      free_name_map(nroot);
      nroot = NULL;
    }

    nroot = read_name_map("name_map");
    if (nroot == NULL) {
        fprintf(stderr, "Unable to read name_map \n");
	*namep = NULL, *cookiep = NULL;
        return(ENOENT);
    }


    *namep = nroot->name;
    *cookiep = nroot;

    return(0);
}

int
nlkup_next_entry(char **namep, void **cookiep)
{
    name_map_t *         cur_map;
    name_map_t *         last_map;

    /*
     * This is pretty ugly, we probably want to keep it
     * global.
     */
    if (nroot == NULL)
	return nlkup_first_entry(namep, cookiep);

    last_map = (name_map_t *) *cookiep;
    cur_map = last_map->next;

    if (cur_map == NULL) {
        *cookiep = NULL;
        return(ENOENT);
    } else {      
        *namep = cur_map->name;
        *cookiep = cur_map;
        return(0);
    }
}



int
nlkupwrap()
{
    return(1);
}

name_map_t *
read_name_map(char *map_file)
{
	int ret;

	yyin = dconfig_open_config_file(map_file);
	if (yyin == NULL) {
		/* XXX log */
		fprintf(stderr, "failed to open name_map file \n");
		return NULL;
	}

	ret = nlkuplex();
	if (ret) {
		fprintf(stderr, "error from yylex\n");
	        free(ninfo.ni_fname);
		return NULL;
	}

	if (err) {
		fprintf(stderr, "error from yylex\n");
		free_name_map(nroot);
		nroot = NULL;
		return NULL;
	}

	return(nroot);
}


void
free_name_map(name_map_t *map) {
	name_map_t *trav, *next;

	for(trav = map; trav != NULL; trav = next) {
		 next = trav->next;
		 free(trav);
	}
}