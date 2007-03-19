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
 
#ifndef CENTRY_H_
#define CENTRY_H_

cache_obj *get_cache_entry(int fd);
void print_cache_entry(cache_obj *ce);
void put_cache_entry(int fd, cache_obj *ce);

cache_init_obj *get_cache_init_entry(int fd);
void print_cache_init_entry(cache_init_obj *ce);
void put_cache_init_entry(int fd, cache_init_obj *ce);

#endif /*CENTRY_H_*/
