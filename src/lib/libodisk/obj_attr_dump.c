/*
 *
 *
 *                          Diamond 1.0
 * 
 *            Copyright (c) 2002-2004, Intel Corporation
 *                         All Rights Reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 *    * Neither the name of Intel nor the names of its contributors may
 *      be used to endorse or promote products derived from this software 
 *      without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <dirent.h>
#include <stdint.h>
#include <string.h>

#include "diamond_types.h"
#include "obj_attr.h"
#include "lib_odisk.h"
#include "odisk_priv.h"




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
