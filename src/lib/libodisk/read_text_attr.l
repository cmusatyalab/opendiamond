%option nounput noyywrap reentrant
%option prefix="textattr"
%{
/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 4
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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>


#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <stddef.h>
#include <ctype.h>
#include <stdint.h>
#include <netdb.h>
#include <assert.h>
#include "diamond_consts.h"
#include "diamond_types.h"
#include "sig_calc.h"
#include "lib_odisk.h"
#include "lib_log.h"
#include "odisk_priv.h"
#include "obj_attr.h"


static void have_value(yyscan_t scanner);
static void have_error(char *input);


struct read_state {
  obj_data_t *cur_obj;
  char *cur_base;
  odisk_state_t *cur_odisk;
  char attr_name[MAX_ATTR_NAME];
  char attr_value[MAX_ATTR_VALUE];
};


%}
NAME	\"[^\"]+\"
VALUE	\"[^\"]*\"
WS	[ \t]*

%%
^#.*\n						{printf("comment\n"); }
\n					
{WS}{NAME}{WS}={WS}{VALUE}.*\n			{ have_value(yyscanner);   }
.						{ have_error(yyget_text(yyscanner)); }
%%


static void 
have_error(char *input)
{
	log_message(LOGT_DISK, LOGL_ERR,
	    "read_text_attr: unknown text (%s)", input);
}


static int
extract_name_value(char *input, char *name, char *value)
{
	int	inlen = strlen(input);
	int	offset = 0;
	int	soffset = 0;

	/* remove leading data before the " */
	while(input[offset] != '"') {
		offset++;
		if (offset >= inlen) return(1);
	}

	/* skip the " */
	offset++;

	/* get the rest of the string */
	soffset = 0;
	while (input[offset] != '"') {
		name[soffset++] = input[offset++];
		if (offset >= inlen) return(1);
		if (soffset >= MAX_ATTR_NAME) {
			log_message(LOGT_DISK, LOGL_ERR,
		    	    "extract_name_value: attr too long (%s)", input);
			return(1);
		}
	}
	name[soffset] = '\0';

	/* skip the " */
	offset++;
	
	/* look for = */
	while (input[offset] != '=') {
		offset++;
		if (offset >= inlen) {
			log_message(LOGT_DISK, LOGL_ERR,
		    	    "extract_name_value: missing '=' (%s)",
			    input);
			return(1);
		}
	}

	/* skip the key value */
	offset++;
	
	/* remove leading data before the " */
	while(input[offset] != '"') {
		offset++;
		if (offset >= inlen)
			return(1);
	}

	/* skip the " */
	offset++;
	
	/* get the rest of the string */
	soffset = 0;
	while (input[offset] != '"') {
		value[soffset++] = input[offset++];
		if (offset >= inlen) {
			return(1);
		}
		if (soffset >= MAX_ATTR_VALUE) {
			log_message(LOGT_DISK, LOGL_ERR,
		    	    "extract_name_value: string too long (%s)",
			    input);
			return(1);
		}
	}
	value[soffset] = '\0';
	return(0);
}



static void
have_value(yyscan_t scanner)
{
	int		err;
	int		dlen;
	struct read_state *extra = (struct read_state *) yyget_extra(scanner);


	err = extract_name_value(yyget_text(scanner),
				 extra->attr_name,
				 extra->attr_value);
	if (err) {
		log_message(LOGT_DISK, LOGL_ERR,
		    "have_value: failed to extract name/value (%s)", input);
		return;
	}

	dlen = strlen((char *)extra->attr_value) +1;
	err = obj_write_attr(&extra->cur_obj->attr_info, extra->attr_name, dlen,
	    (unsigned char *)extra->attr_value);
	if (err) {
		log_message(LOGT_DISK, LOGL_ERR,
		    "have_value: failed to write attribute (%s)",
			    extra->attr_name);
	}
}

static FILE *
open_attr_file(char *fname)
{
	FILE *		new_file;

	/* try the local directory */
	new_file = fopen(fname, "r");
	if (new_file != NULL) {
		return(new_file);
	}	

	return(NULL);
}



void
obj_load_text_attr(odisk_state_t *odisk, char *file_name, obj_data_t *new_obj)
{
	int ret;
	yyscan_t scanner;
	FILE *f;

	struct read_state extra = {
	  .cur_obj = NULL,
	  .cur_base = NULL,
	  .cur_odisk = NULL,
	  .attr_name[0] = '\0',
	  .attr_value[0] = '\0',
	};

	f = open_attr_file(file_name);

	/*
	 * it is possible the file doesn't exist so this is not a real
	 * error.
	 */
	if (f == NULL) {
		log_message(LOGT_DISK, LOGL_TRACE,
		    "obj_load_text_attr: not text attr file (%s)",
		    file_name);
		return;
	}


	/* set visible pointer to the object */
	extra.cur_obj = new_obj;
	extra.cur_base = file_name;
	extra.cur_odisk = odisk;

	yylex_init(&scanner);
	yyset_extra(&extra, scanner);
	yyset_in(f, scanner);
	ret = yylex(scanner);
	yylex_destroy(scanner);
	fclose(f);

	if (ret) {
		log_message(LOGT_DISK, LOGL_ERR,
		    "obj_load_text_attr: failed to process attr file (%s)",
		    file_name);
	}

}
