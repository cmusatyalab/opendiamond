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
#include "name_map.h"
#include "lib_dconfig.h"

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
    name_map_t *         cur_map;
    int                 i;

    /*
     * This is pretty ugly, we probably want to keep it
     * global.
     */
    if (nroot == NULL) {
        /* XXX */
        nroot = read_name_map("name_map");
    }

    if (nroot == NULL) {
        fprintf(stderr, "Unable to find name_map \n");
        assert(0);

        *num_gids = 0;
        return(ENOENT);
    }


    for (cur_map = nroot; cur_map != NULL; cur_map = cur_map->next) {
        if (strcmp(name, cur_map->name) == 0) {
			break;
		}
	}

	if (cur_map == NULL) {
		*num_gids = 0;
		return(ENOENT);
	}


	if (cur_map->num_gids > *num_gids) {
		/* XXX log */
		*num_gids = cur_map->num_gids;
		return(ENOMEM);
	}

	for (i = 0; i < cur_map->num_gids; i++) {
		gids[i] = cur_map->gids[i];
	}
	*num_gids = cur_map->num_gids;

	return(0);
}

int
nlkup_first_entry(char **namep, void **cookiep)
{
    name_map_t *         cur_map;

    /*
     * This is pretty ugly, we probably want to keep it
     * global.
     */
    if (nroot == NULL) {
        /* XXX */
        nroot = read_name_map("name_map");
    }

    if (nroot == NULL) {
        fprintf(stderr, "Unable to find name_map \n");
        assert(0);
        return(ENOENT);
    }


    cur_map = nroot;

    *namep = cur_map->name;
    *cookiep = cur_map;

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
    if (nroot == NULL) {
        /* XXX */
        nroot = read_name_map("name_map");
    }

    if (nroot == NULL) {
        fprintf(stderr, "Unable to find name_map \n");
        assert(0);
        return(ENOENT);
    }


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


#define	MAX_PATH	256
#define DIAMOND_BIN	"DIAMOND_BIN"
#define DIAMOND_DEFAULT	"/usr/local/diamond"
/*
 * This looks for the map file in a couple of places.
 * the first place is in $DIAMOND_BIN/etc, the second
 * place is the local directory, the last place is
 * is /usr/local/diamond/etc.
 */

static FILE *
open_map_file(char *map_name)
{
	FILE *  new_file;
	char *  lkup;
	char	fname[MAX_PATH];


	/* try the environment variable */
	lkup = getenv(DIAMOND_BIN);
	if (lkup != NULL) {
		/* XXX deal with overflow */
		snprintf(fname, MAX_PATH, "%s/%s/%s", lkup, "etc", map_name);
		fname[MAX_PATH - 1] = '\0';
		new_file = fopen(fname, "r");
		if (new_file != NULL) {
            ninfo.ni_fname = strdup(fname);
            assert(ninfo.ni_fname != NULL);
			return(new_file);
		}	
	}

		
	/* try the local directory */
	new_file = fopen(map_name, "r");
	if (new_file != NULL) {
		/* printf("XXX local %s \n", map_name); */
        ninfo.ni_fname = strdup(map_name);
        assert(ninfo.ni_fname != NULL);
		return(new_file);
	}	

	/* try the default absolute path */
	/* XXX deal with overflow */
	snprintf(fname, MAX_PATH, "%s/%s/%s", DIAMOND_DEFAULT, "etc", map_name);
	fname[MAX_PATH - 1] = '\0';
	new_file = fopen(fname, "r");
	if (new_file != NULL) {
        ninfo.ni_fname = strdup(fname);
        assert(ninfo.ni_fname != NULL);
		return(new_file);
	}

	/* didn't find a file, return NULL */
	return(NULL);
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

	yyin = open_map_file(map_file);
	if (yyin == NULL) {
		/* XXX log */
		fprintf(stderr, "failed to open map file \n");
		exit(1);
	}

	ret = nlkuplex();
	if (ret) {
		/* printf("error from yylex\n"); */
        free(ninfo.ni_fname);
		exit(1);
	}

	if (err) {
		/* XXX clean up list */
		return(NULL);
	}

	return(nroot);
}
