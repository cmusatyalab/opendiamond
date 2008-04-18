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

#ifndef _LIB_DCONFIG_H_
#define _LIB_DCONFIG_H_

typedef enum {
	DATA_TYPE_OBJECT = 1,
	DATA_TYPE_NATIVE
} data_type_t;

#ifdef __cplusplus
extern          "C"
{
#endif


/*
 * Name lookup functions that map names into a collection of
 * group ids.
 */

/* returns list of gids for name */
int nlkup_lookup_collection(char *name, int *num_gids, groupid_t * gids);

/* iterates through all names of collections (const name).  */
int nlkup_first_entry(char **name, void **cookie);
int nlkup_next_entry(char **name, void **cookie);


/* Functions that map groups into a set of hosts.  */
int glkup_gid_hosts(groupid_t gid, int *num_hosts, char *hosts[]);


char * dconf_get_dataroot(void);
char * dconf_get_indexdir(void);
char * dconf_get_logdir(void);
char * dconf_get_cachedir(void);
char * dconf_get_spec_cachedir(void);
char * dconf_get_binary_cachedir(void);
char * dconf_get_blob_cachedir(void);
char * dconf_get_filter_cachedir(void);

data_type_t	dconf_get_datatype(void);


#ifdef __cplusplus
}
#endif
#endif                          /* !_LIB_DCONFIG_H_ */

