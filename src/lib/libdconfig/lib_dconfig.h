#ifndef _LIB_DCONFIG_H_
#define _LIB_DCONFIG_H_


#ifdef __cplusplus
extern "C" {
#endif


/*
 * Name lookup functions that map names into a collection of
 * group ids.
 */

/* returns list of gids for name */
int nlkup_lookup_collection(char *name, int *num_gids, groupid_t *gids);

int nlkup_add_entry(char *name, int num_gids, groupid_t *gids);

/* iterates through all names of collections (const name). */
int nlkup_first_entry(char **name, void **cookie);
int nlkup_next_entry(char **name, void **cookie);

/*
 * Functions that map groups into a set of hosts.
 */

int glkup_gid_hosts(groupid_t gid, int *num_hosts, uint32_t *hostids);

#ifdef __cplusplus
}
#endif

#endif  /* !_LIB_DCONFIG_H_ */
