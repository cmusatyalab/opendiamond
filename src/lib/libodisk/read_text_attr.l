%option nounput
%option prefix="textattr"
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


/*
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
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
#include "odisk_priv.h"
#include "obj_attr.h"


static void have_value(char *input);
static void have_ref(char *input);
static void have_error();


static char  attr_name[MAX_ATTR_NAME];
static char  attr_value[MAX_ATTR_NAME];


static obj_data_t *cur_obj;
static char *cur_base;
static odisk_state_t *cur_odisk;


%}
%array
NAME	\"[^\"]+\"
VALUE	\"[^\"]*\"
WS	[ \t]*
HEX16	{HEX8}:{HEX8}
HEX32	{HEX16}:{HEX16}
HEX64	{HEX32}:{HEX32}

%%
^#.*\n						{printf("comment\n"); }
\n					
{WS}{NAME}{WS}={WS}{VALUE}.*\n			{ have_value(yytext);   }
{WS}{NAME}{WS}@{WS}{VALUE}.*\n			{ have_ref(yytext);   }
.						{ have_error(); }
%%


static void 
have_error()
{
	fprintf(stderr, "unknown text %s\n", yytext);
}


int
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
			/* XXX log */
			printf("Data too large for storage \n");
			return(1);
		}
	}
	name[soffset] = '\0';

	/* skip the " */
	offset++;
	
	/* look for = or @ */
	while ((input[offset] != '=') && (input[offset] != '@')) {
		offset++;
		if (offset >= inlen) return(1);
	}
	/* skip the key value */
	offset++;
	
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
		value[soffset++] = input[offset++];
		if (offset >= inlen) return(1);
		if (soffset >= MAX_ATTR_NAME) {
			/* XXX log */
			printf("Data too large for storage \n");
			return(1);
		}
	}
	value[soffset] = '\0';
	return(0);
}



static void
have_value(char *input)
{
	int		err;
	int		dlen;

	err = extract_name_value(input, attr_name, attr_value);
	if (err) {
		fprintf(stderr, "Failed to parse string <%s> \n", input);
	}

	dlen = strlen((char *)attr_value) +1;
	err = obj_write_attr(&cur_obj->attr_info, attr_name, dlen, 
		(unsigned char *)attr_value);
	if (err) {
		fprintf(stderr, "Failed writing attribute \n");
		
	}
}

static int
load_ref_file(char *afile, size_t *dlen, unsigned char **dref)
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
			fprintf(stderr, "load_ref_file: File too long \n");
			return(1);
		}
	} else {
		char *	offset;
		int	remain_len;

		len = snprintf(attr_name, NAME_MAX, "%s", cur_base);
		if (len >= NAME_MAX) {
			fprintf(stderr, "load_ref_file: File too long \n");
			return(1);
		}
		offset = rindex(attr_name, '/');
		if (offset == NULL) {
			fprintf(stderr, "load_ref_file: can't find prefix \n");
			return(1);
		}
	
		remain_len = NAME_MAX - (int)(offset + 1 - attr_name);
		len = snprintf(offset+1, remain_len, "%s", afile);
		if (len >= remain_len) {
			fprintf(stderr, "load_ref_file: complete file long\n");
			return(1);
		}
	}

	/* attr name now holds the name of the file we want */
	printf("attr file <%s> \n", attr_name);


	/* try to open the file */
        os_file = open(attr_name, cur_odisk->open_flags);
        if (os_file == -1) {
                fprintf(stderr, "load_ref_file: can't open file \n");
                return (1);
        }

	err = fstat(os_file, &stats);
        if (err != 0) {
                fprintf(stderr, "load_ref_file: can't stat file \n");
                return (1);
        }
        
	data = (unsigned char *)malloc(stats.st_size);
	if (data == NULL) {
                fprintf(stderr, "load_ref_file: can't alloc mem \n");
                return (1);
	} 

	size = read(os_file, data, stats.st_size);
	if (size != stats.st_size) {
	    	fprintf(stderr, "load_ref_file: failed read \n");
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
have_ref(char *input)
{
	int			err;
	size_t			dlen = 0;
	unsigned char *		data = NULL;


	err = extract_name_value(input, attr_name, attr_value);
	if (err) {
		fprintf(stderr, "have_ref: Failed to parse string <%s> \n", 	
			input);
	}

	err = load_ref_file(attr_value, &dlen, &data);
	if (err) {
		fprintf(stderr, "have_ref: can't load file %s \n", attr_value);
	}

	/* attr value is the file name to load the data */

	err = obj_write_attr(&cur_obj->attr_info, attr_name, dlen, 
		data);
	if (err) {
		fprintf(stderr, "have_ref: Failed writing attribute \n");
	}
	free(data);


}

int textattrwrap() {
	return(1);
}

static FILE *
open_attr_file(char *base_name)
{
	FILE *		new_file;
	char            attr_name[NAME_MAX];
	int		len;


	/* 
	 * Load attributes if any.
	 */
	len = snprintf(attr_name, NAME_MAX, "%s%s", base_name, TEXT_ATTR_EXT);
      	assert(len < NAME_MAX);

	/* try the local directory */
	new_file = fopen(attr_name, "r");
	if (new_file != NULL) {
		/* printf("XXX local %s \n", map_name); */
		return(new_file);
	}	

	return(NULL);

}



void
obj_load_text_attr(odisk_state_t *odisk, char *file_name, obj_data_t *new_obj)
{
	int ret;

	yyin = open_attr_file(file_name);

	/* if no file then exit */
	if (yyin == NULL) {
		return;
	}
	/* set visible pointer to the object */
	cur_obj = new_obj;
	cur_base = file_name;
	cur_odisk = odisk;
		

	ret = yylex();
	if (ret) {
		/* XXX log message */
	}

	fclose(yyin);

}
