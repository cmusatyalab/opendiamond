#ifndef _GID_MAP_H_
#define _GID_MAP_H_

typedef struct  dev_info {
        char *      dev_name;
        uint32_t    dev_id;
} dev_info_t;

#define	MAX_DEV_PER_GROUP	32
typedef struct gid_map {
	struct gid_map *	next;
	groupid_t		    gid;
	int			        num_dev;
	dev_info_t		    devs[MAX_DEV_PER_GROUP];
} gid_map_t;


gid_map_t *read_gid_map(char *mapfile);

#endif  /* !_GID_MAP_H_ */
