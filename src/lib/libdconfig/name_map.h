#ifndef _NAME_MAP_H_
#define _NAME_MAP_H_


#define	MAX_GROUP_PER_NAME	32

typedef struct name_map {
	struct name_map *	next;
    char *              fname;
    char *              name;
	int			        num_gids;
	groupid_t		    gids[MAX_GROUP_PER_NAME];
} name_map_t;

typedef struct name_info {
    char *              ni_fname;   /* full path of the config file */
    name_map_t          ni_nlist;   /* list of name map entries */
} name_info_t;

name_map_t *read_name_map(char *mapfile);

int nlkup_lookup_collection(char *name, int *num_gids, groupid_t *gids);
int nlkup_add_entry(char *name, int num_gids, groupid_t *gids);
int nlkup_first_entry(char **name, void **cookie);
int nlkup_next_entry(char **name, void **cookie);

#endif  /* !_NAME_MAP_H */
