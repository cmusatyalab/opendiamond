
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <dirent.h>
#include <assert.h>
#include "obj_attr.h"
#include "lib_od.h"
#include "lib_odisk.h"
#include "odisk_priv.h"
#include "attr.h"

#define	MAX_FNAME	128


/*
 * These are the set of group ID's we are using to 
 * filter the data.
 */
#define MAX_GID_FILTER  64

static groupid_t   gid_list[MAX_GID_FILTER];
static int         num_gids = 0;




static int
odisk_open_dir(odisk_state_t *odisk)
{
	DIR *			dir;

	dir = opendir(odisk->odisk_path);
	if (dir == NULL) {
		/* XXX log */
		printf("failed to open %s \n", odisk->odisk_path);
		return(ENOENT);
	}

	odisk->odisk_dir = dir;

	return(0);
}


int
odisk_load_obj(obj_data_t  **obj_handle, char *name)
{
	obj_data_t *	new_obj;
	struct stat	stats;
	FILE	 *	os_file;
	char *		data;
	int		err;
	size_t		size;
    char        attr_name[MAX_ATTR_NAME];


	/* XXX printf("load_obj: <%s> \n", name); */

	if (strlen(name) >= MAX_FNAME) {
		/* XXX log error */
		return (EINVAL);
	}

	new_obj = malloc(sizeof(*new_obj));
	if (new_obj == NULL) {
		/* XXX log error */
		return (ENOMEM);
	}

	err = stat(name, &stats);
	if (err != 0) {
		free(new_obj);
		return(ENOENT);
	}
		

	/* open the file */
	os_file  = fopen(name, "rb");
	if (os_file == NULL) {
		free(new_obj);
		return (ENOENT);
	}

	data = (char *)malloc(stats.st_size);
	if (data == NULL) {
		fclose(os_file);
		free(new_obj);
		return (ENOENT);

	}

    if (stats.st_size > 0) {
	    size = fread(data, stats.st_size, 1, os_file);
	    if (size != 1) {
		    /* XXX log error */
		    printf("odisk_load_obj: failed to reading data \n");
		    free(data);
		    fclose(os_file);
		    free(new_obj);
		    return (ENOENT);
	    }
    } 
	new_obj->data = data;
	new_obj->data_len = stats.st_size;

    /*
     * Load the attributes, if any.
     */
    /* XXX overflow */
    sprintf(attr_name, "%s%s", name, ATTR_EXT);
    obj_read_attr_file(attr_name , &new_obj->attr_info);

	
	*obj_handle = (obj_data_t *)new_obj;

	fclose(os_file);
	return(0);
}


int
odisk_get_obj_cnt(odisk_state_t *odisk)
{
	struct dirent *		cur_ent;
	int			extlen, flen;
	char *			poss_ext;
	DIR *		dir;
	int		count = 0;

	dir = opendir(odisk->odisk_path);
	if (dir == NULL) {
		/* XXX log */
		printf("failed to open %s \n", odisk->odisk_path);
		return(0);
	}


	while (1) {

		cur_ent = readdir(dir);
		/*
		 * If readdir fails, then we have enumerated all
		 * the contents.
		 */

		if (cur_ent == NULL) {
			closedir(dir);
			return(count);
		}

		/*
	 	 * If this isn't a file then we skip the entry.
	 	 */
		if ((cur_ent->d_type != DT_REG) && 
				(cur_ent->d_type != DT_LNK)) {
			continue;
		}

		/*
	 	 * If this entry ends with the string defined by ATTR_EXT,
	 	 * then this is not a data file but an attribute file, so
	 	 * we skip it.
	 	 */
		extlen = strlen(ATTR_EXT);
		flen = strlen(cur_ent->d_name);
		if (flen > extlen) {
			poss_ext = &cur_ent->d_name[flen - extlen];
			if (strcmp(poss_ext, ATTR_EXT) == 0) {
				continue;
			}
		} 
		/* if we get here, this is a good one */
		count++;
	}

	closedir(dir);

}

int
odisk_save_obj(odisk_state_t *odisk, obj_data_t *obj)
{
    char        buf[MAX_FNAME];
    char        attrbuf[MAX_FNAME];
	FILE	 *	os_file;
    int         size;

    sprintf(buf, "%s/OBJ%016llX", odisk->odisk_path, obj->local_id);
		

	/* open the file */
	os_file  = fopen(buf, "wb");
	if (os_file == NULL) {
        printf("Failed to open save obj \n");
		return (ENOENT);
	}

    if (obj->data_len > 0) {
	    size = fwrite(obj->data, obj->data_len, 1, os_file);
	    if (size != 1) {
		    /* XXX log error */
		    printf("failed to write data \n");
		    return (ENOENT);
	    }
    }


    sprintf(attrbuf, "%s%s", buf, ATTR_EXT);

    obj_write_attr_file(attrbuf, &obj->attr_info);


	fclose(os_file);
	return(0);
}


int
odisk_get_obj(odisk_state_t *odisk, obj_data_t **obj, obj_id_t *oid)
{
    char    buf[120];
    int     err;

    sprintf(buf, "%s/OBJ%016llX", odisk->odisk_path, oid->local_id);

    err = odisk_load_obj(obj, buf);
    (*obj)->local_id = oid->local_id;
    return(err);
}


int
odisk_release_obj(odisk_state_t *odisk, obj_data_t *obj)
{

    if (obj->data != NULL) {
        free(obj->data);
    }

    if (obj->attr_info.attr_data != NULL) {
        free(obj->attr_info.attr_data);
    }

    free(obj);
    return(0);
}


int
odisk_add_gid(odisk_state_t *odisk, obj_data_t *obj, groupid_t *gid)
{
    gid_list_t* glist;
    off_t       len; 
    int         i, err;
    int         space;

    len = 0;
    err = obj_read_attr(&obj->attr_info, GIDLIST_NAME, &len, NULL);
    if (err == ENOENT) {
        glist = (gid_list_t *) malloc(GIDLIST_SIZE(4));
        assert(glist != NULL);
        memset(glist, 0, GIDLIST_SIZE(4));
    } else if (err != ENOMEM) {
        return(err);
    } else {
        glist = (gid_list_t *)malloc(len);
        err = obj_read_attr(&obj->attr_info, GIDLIST_NAME, &len, (char *)glist);
        assert(err == 0);
    }

    space = -1;
    for (i=0; i < glist->num_gids; i++) {
        if ((glist->gids[i] == 0) && (space == -1)) {
            space = i;
        }
        if (glist->gids[i] == *gid) {
            return(EAGAIN);
        }
    }
    
    if (space == -1) {
        int old, new;
        old = glist->num_gids;
        new = old + 4;
        glist = realloc(glist, GIDLIST_SIZE(new));
        assert(glist != NULL);

        for (i = old; i < new; i++) {
            glist->gids[i] = 0;
        }
        glist->num_gids = new;
        space = old;
    }

    glist->gids[space] = *gid;
    err = obj_write_attr(&obj->attr_info, GIDLIST_NAME,     
                    GIDLIST_SIZE(glist->num_gids), (char *)glist);
    assert(err == 0);
    return(0);
}


int
odisk_rem_gid(odisk_state_t *odisk, obj_data_t *obj, groupid_t *gid)
{
    gid_list_t* glist;
    off_t       len; 
    int         i, err;

    len = 0;
    err = obj_read_attr(&obj->attr_info, GIDLIST_NAME, &len, NULL);
    if (err != ENOMEM) {
        return(err);
    }

    glist = (gid_list_t *)malloc(len);
    err = obj_read_attr(&obj->attr_info, GIDLIST_NAME, &len, (char *)glist);
    assert(err == 0);
   

    for (i=0; i < glist->num_gids; i++) {
        if (glist->gids[i] == *gid) {
            glist->gids[i] = *gid;
            err = obj_write_attr(&obj->attr_info, GIDLIST_NAME,     
                    GIDLIST_SIZE(glist->num_gids), (char *)glist);
            return(0);
        }
    }
    
    return(ENOENT);
}

int
odisk_new_obj(odisk_state_t *odisk, obj_id_t*  oid, groupid_t *gid)
{
    char                buf[120];
    uint64_t            local_id;
    int                 fd;
    obj_data_t  *       obj;

    local_id = 1;

    while (1) {

        sprintf(buf, "%s/OBJ%016llX", odisk->odisk_path, local_id);

        fd = open(buf, O_CREAT|O_EXCL, 0777); 

        if (fd == -1) {
            local_id++;
        } else {
            break;
        }
    }

    oid->local_id = local_id;
    close(fd);


    odisk_get_obj(odisk, &obj, oid);
    odisk_add_gid(odisk, obj, gid);
    odisk_save_obj(odisk, obj);
    odisk_release_obj(odisk, obj);

    return(0);
}

int
odisk_clear_gids(odisk_state_t *odisk, groupid_t gid)
{
	num_gids = 0;
    	return(0);
}

int
odisk_set_gid(odisk_state_t *odisk, groupid_t gid)
{

    if (num_gids >= MAX_GID_FILTER) {
        return(ENOMEM);
    }

    gid_list[num_gids] = gid;
    num_gids++;
    return(0);
}
        
int
odisk_write_obj(odisk_state_t *odisk, obj_data_t *obj, int len,
                int offset, char *data)
{
    int     total_len;
    char *  dbuf;

    total_len = offset + len;

    if (total_len > obj->data_len) {
        dbuf = realloc(obj->data, total_len);
        assert(dbuf != NULL);
        obj->data_len = total_len;
        obj->data = dbuf;
    }

    memcpy(&obj->data[offset], data, len);

    return(0);
}


int
odisk_read_obj(odisk_state_t *odisk, obj_data_t *obj, int *len,
                int offset, char *data)
{
    int     rlen;
    int     remain;

    if (offset >= obj->data_len) {
        *len = 0;
        return(0);
    }
   
    remain = obj->data_len - offset;
    if (remain > *len) {
        rlen = *len;
    } else {
        rlen = remain;
    }


    memcpy(data, &obj->data[offset], rlen);

    *len = rlen;

    return(0);
}


static int
odisk_gid_good(obj_data_t *obj)
{
    gid_list_t* glist;
    off_t       len; 
    int         i,j, err;

    len = 0;
    err = obj_read_attr(&obj->attr_info, GIDLIST_NAME, &len, NULL);
    if (err != ENOMEM) {
        return(err);
    }

    glist = (gid_list_t *)malloc(len);
    err = obj_read_attr(&obj->attr_info, GIDLIST_NAME, &len, (char *)glist);
    assert(err == 0);

    for (i=0; i < glist->num_gids; i++) {
        for (j=0; j < num_gids; j++) {
            if ((glist->gids[i] == gid_list[j])) {
                free(glist);
                return(0);
            }
        }
    }


    free(glist);
    return(ENOENT);
}


int
odisk_next_obj(obj_data_t **new_object, odisk_state_t *odisk)
{
	struct dirent *		cur_ent;
	char			path_name[NAME_MAX];
	int			err;
	int			extlen, flen;
	char *			poss_ext;


next:
	cur_ent = readdir(odisk->odisk_dir);
	if (cur_ent == NULL) {
		/* printf("no ent !! \n"); */
		return(ENOENT);
	}

	/*
	 * If this isn't a file then we skip the entry.
	 */
	if ((cur_ent->d_type != DT_REG) && (cur_ent->d_type != DT_LNK)) {
		/* printf("not regular file %s \n", cur_ent->d_name); */
		goto next;
	}

	/*
	 * If this entry ends with the string defined by ATTR_EXT,
	 * then this is not a data file but an attribute file, so
	 * we skip it.
	 */
	extlen = strlen(ATTR_EXT);
	flen = strlen(cur_ent->d_name);
	if (flen > extlen) {
		poss_ext = &cur_ent->d_name[flen - extlen];
		if (strcmp(poss_ext, ATTR_EXT) == 0) {
			goto next;
		}
	}

	sprintf(path_name, "%s/%s", odisk->odisk_path, cur_ent->d_name);

	err = odisk_load_obj(new_object, path_name);
	if (err) {
		/* XXX log */
		printf("create obj failed %d \n", err);
		return(err);
	}

    err = odisk_gid_good(*new_object);
    if (err) {
        odisk_release_obj(odisk, *new_object);
        goto next;
    }

	/* XXX */
	obj_write_attr(&((*new_object)->attr_info),
		       OBJ_PATH, strlen(path_name)+1, path_name);


	return(0);
}



int
odisk_init(odisk_state_t **odisk, char *dir_path)
{
	int	err;
	odisk_state_t  *	new_state;

	if (strlen(dir_path) > (MAX_DIR_PATH-1)) {
		/* XXX log */
		return(EINVAL);
	}


	new_state = (odisk_state_t *)malloc(sizeof(*new_state));
	if (new_state == NULL) {
		/* XXX err log */
		return(ENOMEM);
	}

	/* the length has already been tested above */
	strcpy(new_state->odisk_path, dir_path);	


	err = odisk_open_dir(new_state);
	if (err != 0) {
		free(new_state);
		/* XXX log */
		printf("failed to init device emulation \n");
		return (err);
	}
	*odisk = new_state;
	return(0);
}






int
odisk_term(odisk_state_t *odisk)
{
	int	err;


	err = closedir(odisk->odisk_dir);

	odisk->odisk_dir = NULL;

	free(odisk);
	return (err);
}

static void
update_gid_idx(odisk_state_t *odisk, char *name, groupid_t gid)
{
	char	idx_name[256];
	FILE *	idx_file;
	int	num;
	gid_idx_ent_t	gid_idx;

	sprintf(idx_name, "%s/%s%016llX", odisk->odisk_path, GID_IDX, gid);

	idx_file = fopen(idx_name, "a");
	if (idx_file == NULL) {
		fprintf(stderr, "update_gid_idx: failed to open <%s> \n", 
			idx_name);
		return;
	}

	memset(&gid_idx, 0, sizeof(gid_idx));
	sprintf(gid_idx.gid_name, "%s", name);

	num = fwrite(&gid_idx, sizeof(gid_idx), 1, idx_file);
	assert(num == 1);

	fclose(idx_file);
}

static void
update_object_gids(odisk_state_t *odisk, obj_data_t *obj, char *name)
{
    gid_list_t* glist;
    off_t       len; 
    int         i, err;

    len = 0;
    err = obj_read_attr(&obj->attr_info, GIDLIST_NAME, &len, NULL);
    if (err != ENOMEM) {
	/* XXX log ?? */
        return;
    }

    glist = (gid_list_t *)malloc(len);
    err = obj_read_attr(&obj->attr_info, GIDLIST_NAME, &len, (char *)glist);
    assert(err == 0);

    for (i=0; i < glist->num_gids; i++) {
	update_gid_idx(odisk, name, glist->gids[i]);
    }

    free(glist);
}


/*
 * Clear all of the GID index files.  
 */
 
int
odisk_clear_indexes(odisk_state_t *odisk)
{
	struct dirent *		cur_ent;
	int			extlen, flen;
	char *			poss_ext;
	DIR *		dir;
	int		count = 0;
	int			err;

	dir = opendir(odisk->odisk_path);
	if (dir == NULL) {
		/* XXX log */
		printf("failed to open %s \n", odisk->odisk_path);
		return(0);
	}


	while (1) {

		cur_ent = readdir(dir);
		/*
		 * If readdir fails, then we have enumerated all
		 * the contents.
		 */

		if (cur_ent == NULL) {
			closedir(dir);
			return(count);
		}

		/*
	 	 * If this isn't a file then we skip the entry.
	 	 */
		if ((cur_ent->d_type != DT_REG) && 
				(cur_ent->d_type != DT_LNK)) {
			continue;
		}

		/*
 		 * If this begins with the prefix GID_IDX, then
 		 * it is an index file so it should be deleted.
 		 */

		flen = strlen(cur_ent->d_name);
		extlen = strlen(GID_IDX);
		if (flen > extlen) {
			poss_ext = &cur_ent->d_name[flen - extlen];
			if (strncmp(cur_ent->d_name, GID_IDX, extlen) == 0) {
				char 	max_path[256];	/* XXX */
    			     
			        sprintf(max_path, "%s/%s", odisk->odisk_path,
					 cur_ent->d_name);
				err = remove(max_path);
				if (err == -1) {
					fprintf(stderr, "Failed to remove %s\n",
						max_path);
				}
			}
		}
	}

	closedir(dir);
}

int
odisk_build_indexes(odisk_state_t *odisk)
{
	struct dirent *		cur_ent;
	int			extlen, flen;
	char *			poss_ext;
	char 	max_path[256];	/* XXX */
	DIR *		dir;
	int		count = 0;
	int			err;
	obj_data_t	 *	new_object;

	dir = opendir(odisk->odisk_path);
	if (dir == NULL) {
		/* XXX log */
		printf("failed to open %s \n", odisk->odisk_path);
		return(0);
	}


	while (1) {

		cur_ent = readdir(dir);
		/*
		 * If readdir fails, then we have enumerated all
		 * the contents.
		 */

		if (cur_ent == NULL) {
			closedir(dir);
			return(count);
		}

		/*
	 	 * If this isn't a file then we skip the entry.
	 	 */
		if ((cur_ent->d_type != DT_REG) && 
				(cur_ent->d_type != DT_LNK)) {
			continue;
		}

		/* make sure this isn't an index file */
		flen = strlen(cur_ent->d_name);
		extlen = strlen(GID_IDX);
		if (flen > extlen) {
			if (strncmp(cur_ent->d_name, GID_IDX, extlen) == 0) {
				continue;
			}
		}

		/* see if this is an attribute file */	
		extlen = strlen(ATTR_EXT);
		flen = strlen(cur_ent->d_name);
		if (flen > extlen) {
			poss_ext = &cur_ent->d_name[flen - extlen];
			if (strcmp(poss_ext, ATTR_EXT) == 0) {
				continue;
			}
		}

		sprintf(max_path, "%s/%s", odisk->odisk_path, cur_ent->d_name);

		err = odisk_load_obj(&new_object, max_path);
		if (err) {
			/* XXX log */
			fprintf(stderr, "create obj failed %d \n", err);
			return(err);
		}

		/*
		 * Go through each of the GID's and update the index file.
		 */
      
		update_object_gids(odisk, new_object, cur_ent->d_name);
  
		odisk_release_obj(odisk, new_object);



	}

	closedir(dir);
}
