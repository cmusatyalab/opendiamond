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


#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <dirent.h>
#include <stdint.h>
#include <string.h>

#include "diamond_consts.h"
#include "diamond_types.h"
#include "obj_attr.h"
#include "lib_odisk.h"
#include "odisk_priv.h"

static char const cvsid[] = "$Header$";


typedef void (*sprintf_func_t)(char *, char *);

/* obviously a big hack... */
void
obj_dump_attr(obj_attr_t *attr)
{
	int			cur_offset;
	attr_record_t *		cur_rec = NULL;
	char *name;
	char *val, *typ;
	char *error, buf[BUFSIZ];
	sprintf_func_t func;
	void *handle;

	handle = dlopen("./fdebug.so", RTLD_NOW);
	if (!handle) {
		/* XXX error log */
		//fputs(dlerror(), stderr);
		//exit (1);
	}

#ifdef	XXX
	cur_offset = 0;
	while (cur_offset < attr->attr_len) {
		cur_rec = (attr_record_t *)&attr->attr_data[cur_offset];
		if ((cur_rec->flags & ATTR_FLAG_FREE) == 0) {
			name = cur_rec->data;
			if(handle && name[0] == '_') {
				val = name + cur_rec->name_len;
				for(typ = name + 1;
			    (*typ != '.') && (typ < val); typ++) {}
				if(typ >= val) {
					printf("<%s>\n", name);
				} else {
					sprintf(buf, "%s_sprint", ++typ);
					func = dlsym(handle, buf);
					if ((error = dlerror()) != NULL) {
						fprintf(stderr, "%s on <%s> \n", error, buf);
						sprintf(buf, "?");
					} else {
						func(buf, val);
					}
					printf("<%s> = %s\n", name, buf);
				}
			} else if(( !strcmp(name, "Display-Name") ) ||
			          ( !strcmp(name, "Keywords") ) ||
			          ( !strcmp(name, "Content-Type") )) {
				printf("<%.*s> = %.*s\n", cur_rec->name_len, name,
				       cur_rec->data_len, name + cur_rec->name_len);
			} else {
				printf("<%s>\n", name);
			}
		}

		/* advance */
		cur_offset += cur_rec->rec_len;
	}
#endif
}
