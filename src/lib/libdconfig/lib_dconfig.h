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


#ifndef _LIB_DCONFIG_H_
#define _LIB_DCONFIG_H_


#ifdef __cplusplus
extern          "C"
{
#endif


	/*
	 * Name lookup functions that map names into a collection of
	 * group ids.
	 */

	/* returns list of gids for name */
	int             nlkup_lookup_collection(char *name, int *num_gids,
	                                        groupid_t * gids);

	int             nlkup_add_entry(char *name, int num_gids,
	                                groupid_t * gids);

	/* iterates through all names of collections (const name).  */
	int             nlkup_first_entry(char **name, void **cookie);
	int             nlkup_next_entry(char **name, void **cookie);


	/* Functions that map groups into a set of hosts.  */
	int             glkup_gid_hosts(groupid_t gid, int *num_hosts,
	                                uint32_t * hostids);

#ifdef __cplusplus
}
#endif
#endif                          /* !_LIB_DCONFIG_H_ */
