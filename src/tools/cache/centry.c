/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 2
 *
 *  Copyright (c) 2002-2007 Intel Corporation
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <dirent.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "diamond_types.h"
#include "lib_tools.h"
#include "lib_dconfig.h"
#include "lib_odisk.h"
#include "lib_filterexec.h"
#include "lib_ocache.h"

static char const cvsid[] = "$Header$";

/*
 * Read and dump cache files.
 */
void print_cache_attrs(cache_attr_set *ca) 
{
	int i;
	cache_attr_entry *cae;
	
	for (i = 0; i < ca->entry_num; i++) {
		cae = ca->entry_data[i];
		printf("\t\t%s signature: %s\n",
				cae->the_attr_name, 
				sig_string(&cae->attr_sig));
	}
}


void print_cache_entry(cache_obj *ce) 
{
	printf("Object id: %s\n\tResult: %d\n",
			sig_string(&ce->id_sig), ce->result);
	printf("\tEvaluations: %u\n\tHit count: %u\n",
			ce->eval_count, ce->hit_count);
	printf("\tInput attribute signature: %s\n",
			sig_string(&ce->iattr_sig));
	printf("\tInput attributes:\n");			
	print_cache_attrs(&ce->iattr);
	printf("\tOutput attributes:\n");			
	print_cache_attrs(&ce->oattr);
	printf("\tCreated by %s connect time %u.%u cid %d query %d\n", 
			inet_ntoa(ce->qid.session.clientaddr.sin_addr),
			(unsigned int) ce->qid.session.connect_time.tv_sec, 
			(unsigned int) ce->qid.session.connect_time.tv_usec,
			ce->qid.session.conn_idx, ce->qid.query_id);
	printf("\tExecution mode: %s\n", 
			ce->exec_mode==FM_CURRENT?"Current":
				(ce->exec_mode==FM_HYBRID?"Hybrid":"Model"));
}


cache_obj *get_cache_entry(int fd) 
{
	cache_obj *cobj;
	int rc;
	int i;
	
	cobj = (cache_obj *) calloc(1, sizeof(*cobj));
	assert(cobj != NULL);
	rc = read(fd, &cobj->id_sig, sizeof(sig_val_t));
	if (rc == 0) {
		return NULL;
	} else if (rc < 0) {
		printf("read error \n");
		return NULL;
	}
	rc = read(fd, &cobj->iattr_sig, sizeof(sig_val_t));
	assert(rc == sizeof(sig_val_t));
	rc = read(fd, &cobj->result, sizeof(int));
	assert(rc == sizeof(int));

	rc = read(fd, &cobj->eval_count, sizeof(unsigned short));
	assert(rc == sizeof(unsigned short));
	cobj->aeval_count = 0;
	rc = read(fd, &cobj->hit_count, sizeof(unsigned short));
	assert(rc == sizeof(unsigned short));
	cobj->ahit_count = 0;

	rc = read(fd, &cobj->qid, sizeof(query_info_t));
	assert(rc == sizeof(query_info_t));
	rc = read(fd, &cobj->exec_mode, sizeof(filter_exec_mode_t));
	assert(rc == sizeof(filter_exec_mode_t));

	rc = read(fd, &cobj->iattr.entry_num, sizeof(unsigned int));
	assert(rc == sizeof(unsigned int));
	cobj->iattr.entry_data =
	    calloc(1, cobj->iattr.entry_num * sizeof(char *));
	assert(cobj->iattr.entry_data != NULL);
	for (i = 0; i < cobj->iattr.entry_num; i++) {
		cobj->iattr.entry_data[i] =
		    calloc(1, sizeof(cache_attr_entry));
		assert(cobj->iattr.entry_data[i] != NULL);

		rc = read(fd, cobj->iattr.entry_data[i],
			  sizeof(cache_attr_entry));
		assert(rc == sizeof(cache_attr_entry));

	}

	rc = read(fd, &cobj->oattr.entry_num, sizeof(unsigned int));
	assert(rc == sizeof(unsigned int));

	cobj->oattr.entry_data =
	    calloc(1, cobj->oattr.entry_num * sizeof(char *));
	assert(cobj->oattr.entry_data != NULL);

	for (i = 0; i < cobj->oattr.entry_num; i++) {
		cobj->oattr.entry_data[i] =
		    calloc(1, sizeof(cache_attr_entry));
		assert(cobj->oattr.entry_data[i] != NULL);

		rc = read(fd, cobj->oattr.entry_data[i],
			  sizeof(cache_attr_entry));
		assert(rc == sizeof(cache_attr_entry));
	}
	
	return(cobj);
}

void put_cache_entry(int fd, cache_obj *cobj) 
{
	int count;
	int i;
	
	write(fd, &cobj->id_sig, sizeof(sig_val_t));
	write(fd, &cobj->iattr_sig, sizeof(sig_val_t));
	write(fd, &cobj->result, sizeof(int));

	count = cobj->eval_count + cobj->aeval_count;
	write(fd, &count, sizeof(unsigned short));
	count = cobj->hit_count + cobj->ahit_count;
	write(fd, &count, sizeof(unsigned short));
	
	write(fd, &cobj->qid, sizeof(query_info_t));
	write(fd, &cobj->exec_mode, sizeof(filter_exec_mode_t));
	
	write(fd, &cobj->iattr.entry_num,
	      sizeof(unsigned int));
	for (i = 0; i < cobj->iattr.entry_num; i++) {
		write(fd, cobj->iattr.entry_data[i],
		      sizeof(cache_attr_entry));
	}
	write(fd, &cobj->oattr.entry_num,
	      sizeof(unsigned int));

	for (i = 0; i < cobj->oattr.entry_num; i++) {
		write(fd, cobj->oattr.entry_data[i],
		      sizeof(cache_attr_entry));
	}
}

cache_init_obj *get_cache_init_entry(int fd) 
{
	cache_init_obj *cobj;
	int rc;
	int i;

	cobj = (cache_init_obj *) calloc(1, sizeof(*cobj));
	assert(cobj != NULL);
	rc = read(fd, &cobj->id_sig, sizeof(sig_val_t));
	
	if (rc == 0) {
		return NULL;
	} else if (rc < 0) {
		printf("read error \n");
		return NULL;
	}
	
	assert(rc == sizeof(sig_val_t));
	rc = read(fd, &cobj->attr.entry_num, sizeof(unsigned int));
	assert(rc == sizeof(unsigned int));

	cobj->attr.entry_data =
	    calloc(1, cobj->attr.entry_num * sizeof(char *));
	assert(cobj->attr.entry_data != NULL);

	for (i = 0; i < cobj->attr.entry_num; i++) {
		cobj->attr.entry_data[i] =
		    calloc(1, sizeof(cache_attr_entry));
		assert(cobj->attr.entry_data[i] != NULL);
		rc = read(fd, cobj->attr.entry_data[i],
			      sizeof(cache_attr_entry));
		assert(rc == sizeof(cache_attr_entry));
	}
	
	return(cobj);
}

void print_cache_init_entry(cache_init_obj *ce) 
{
	printf("Object id: %s\n",
			sig_string(&ce->id_sig));
	printf("\tAttributes:\n");			
	print_cache_attrs(&ce->attr);
}

void put_cache_init_entry(int fd, cache_init_obj *cobj) 
{
	int i;
	
	write(fd, &cobj->id_sig, sizeof(sig_val_t));
	write(fd, &cobj->attr.entry_num, sizeof(unsigned int));
	for (i = 0; i < cobj->attr.entry_num; i++) {
		write(fd, cobj->attr.entry_data[i],
			 sizeof(cache_attr_entry));
	}
}


