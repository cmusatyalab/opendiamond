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
static void have_ref(yyscan_t scanner);
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
{WS}{NAME}{WS}@{WS}{VALUE}.*\n			{ have_ref(yyscanner);   }
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
	
	/* look for = or @ */
	while ((input[offset] != '=') && (input[offset] != '@')) {
		offset++;
		if (offset >= inlen) {
			log_message(LOGT_DISK, LOGL_ERR,
		    	    "extract_name_value: missing '=' or '@' (%s)",
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

static int
load_ref_file(char *afile, size_t *dlen, unsigned char **dref,
	      char *cur_base, odisk_state_t *cur_odisk)
{
	struct stat     stats;
	char            attr_name[NAME_MAX];
	int		len;
	int		os_file;
	unsigned char * data;
	size_t		size;
	int		err;

	/* if leading / then this is absolute path */
	if (afile[0] == '/') {
		len = snprintf(attr_name, NAME_MAX, "%s", afile);
		if (len >= NAME_MAX) {
			log_message(LOGT_DISK, LOGL_ERR,
		    	   "load_ref_file: filename too long (%s)", attr_name);
			return(1);
		}
	} else {
		char *	offset;
		int	remain_len;

		len = snprintf(attr_name, NAME_MAX, "%s", cur_base);
		if (len >= NAME_MAX) {
			log_message(LOGT_DISK, LOGL_ERR,
		    	   "load_ref_file: filename too long (%s)", attr_name);
			return(1);
		}
		offset = rindex(attr_name, '/');
		if (offset == NULL) {
			log_message(LOGT_DISK, LOGL_ERR,
		    	   "load_ref_file: no prefix (%s)", attr_name);
			return(1);
		}
	
		remain_len = NAME_MAX - (int)(offset + 1 - attr_name);
		len = snprintf(offset+1, remain_len, "%s", afile);
		if (len >= remain_len) {
			log_message(LOGT_DISK, LOGL_ERR,
		    	   "load_ref_file: file name too long (%s)", attr_name);
			return(1);
		}
	}

	/* try to open the file */
        os_file = open(attr_name, cur_odisk->open_flags);
        if (os_file == -1) {
		log_message(LOGT_DISK, LOGL_ERR,
	       	    "load_ref_file: failed to open (%s)", attr_name);
                return (1);
        }

	err = fstat(os_file, &stats);
        if (err != 0) {
		log_message(LOGT_DISK, LOGL_ERR,
	       	    "load_ref_file: failed to stat file (%s)", attr_name);
                return (1);
        }
        
	data = (unsigned char *)malloc(stats.st_size);
	if (data == NULL) {
		log_message(LOGT_DISK, LOGL_ERR,
	       	    "load_ref_file: failed to malloc %d bytes",
		    stats.st_size);
                return (1);
	} 

	size = read(os_file, data, stats.st_size);
	if (size != stats.st_size) {
		log_message(LOGT_DISK, LOGL_ERR,
	       	    "load_ref_file: failed to read data");
		free(data);
		close(os_file);
		return (1);
	}
        close(os_file);

	*dlen = stats.st_size;
	*dref = data;

	return(0);
}


static void
have_ref(yyscan_t scanner)
{
	int			err;
	size_t			dlen = 0;
	unsigned char *		data = NULL;
	struct read_state *extra = (struct read_state *) yyget_extra(scanner);

	err = extract_name_value(yyget_text(scanner),
				 extra->attr_name, extra->attr_value);
	if (err) {
		log_message(LOGT_DISK, LOGL_ERR,
	       	    "have_ref: failed extract name value from (%s)", input);
		return;
	}

	err = load_ref_file(extra->attr_value, &dlen, &data,
			    extra->cur_base, extra->cur_odisk);
	if (err) {
		log_message(LOGT_DISK, LOGL_ERR,
	       	    "have_ref: failed to load ref file (%s)",
			    extra->attr_value);
	}

	/* attr value is the file name to load the data */

	err = obj_write_attr(&extra->cur_obj->attr_info, extra->attr_name, dlen,
		data);
	if (err) {
		log_message(LOGT_DISK, LOGL_ERR,
	       	    "have_ref: failed to write attributes (%s)",
			    extra->attr_name);
	}
	free(data);
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
