
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
#include "rtimer.h"

#define	MAX_FNAME	128


                                                                                
/* call to read the cycle counter */
#define rdtscll(val) __asm__ __volatile__("rdtsc" : "=A" (val))
                                                                                
                                                                                
static unsigned long long
read_cycle()
{
        unsigned long long      foo;
                                                                                
        rdtscll(foo);
                                                                                
        return(foo);
                                                                                
}


/*
 * These are the set of group ID's we are using to 
 * filter the data.
 */
#define MAX_GID_FILTER  64

int
odisk_load_obj(obj_data_t  **obj_handle, char *name)
{
	obj_data_t *	new_obj;
	struct stat	stats;
	FILE	 *	os_file;
	char *		data;
	int		err;
	size_t		size;
    	char        	attr_name[MAX_ATTR_NAME];
	unsigned long long	tstart, lstart, end;
	


	tstart = read_cycle();

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

	lstart = read_cycle();

	err = stat(name, &stats);
	if (err != 0) {
		free(new_obj);
		return(ENOENT);
	}

	end = read_cycle();
	printf("stat time %lld \n", (end -lstart));		

	/* open the file */
	lstart = read_cycle();
	os_file  = fopen(name, "rb");
	if (os_file == NULL) {
		free(new_obj);
		return (ENOENT);
	}
	end = read_cycle();
	printf("open time %lld \n", (end -lstart));		

	data = (char *)malloc(stats.st_size);
	if (data == NULL) {
		fclose(os_file);
		free(new_obj);
		return (ENOENT);

	}

	lstart = read_cycle();
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

	end = read_cycle();
	printf("data read %lld \n", (end - lstart));		
    	/*
     	 * Load the attributes, if any.
     	 */
    	/* XXX overflow */
    	sprintf(attr_name, "%s%s", name, ATTR_EXT);

	lstart = read_cycle();
    	obj_read_attr_file(attr_name , &new_obj->attr_info);

	end = read_cycle();
	printf("attr read %lld\n", (end  - lstart));		
	
	*obj_handle = (obj_data_t *)new_obj;

	fclose(os_file);

	end = read_cycle();
	printf("total time %lld\n", (end - tstart));		
	return(0);
}


int
odisk_get_obj_cnt(odisk_state_t *odisk)
{
	int		count = 0;
	char	idx_file[256];	/* XXX */
	FILE *	new_file;
	gid_idx_ent_t		gid_ent;
	int	i;

	

	for (i=0; i < odisk->num_gids; i++) {
		sprintf(idx_file, "%s/%s%016llX", odisk->odisk_path, GID_IDX, 
			odisk->gid_list[i]);
		new_file = fopen(idx_file, "r");
		if (new_file == NULL) {
			continue;
		}
		while (fread(&gid_ent, sizeof(gid_ent), 1, new_file) == 1) {
			count++;
		}
		fclose(new_file);
	}

	return(count);
	
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
odisk_clear_gids(odisk_state_t *odisk)
{
	odisk->num_gids = 0;
    	return(0);
}

int
odisk_set_gid(odisk_state_t *odisk, groupid_t gid)
{
	int	i;

	/*
	 * make sure this GID is not already in the list 
	 */
	for (i=0; i < odisk->num_gids; i++) {
		if (odisk->gid_list[i] == gid) {
			return(0);
		}
	}

	/*
	 * make sure there is room for this new entry.
	 */
    	if (odisk->num_gids >= MAX_GID_FILTER) {
       		 return(ENOMEM);
    	}

    	odisk->gid_list[odisk->num_gids] = gid;
    	odisk->num_gids++;
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

/* XXX shared state */
int		search_active = 0;
int		search_done = 0;
int		bg_wait_q = 0;
int		fg_wait = 0;
obj_data_t	*obj_q = NULL;
pthread_mutex_t	shared_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t	fg_data_cv = PTHREAD_COND_INITIALIZER;
pthread_cond_t	bg_active_cv = PTHREAD_COND_INITIALIZER;
pthread_cond_t	bg_queue_cv = PTHREAD_COND_INITIALIZER;


int
odisk_read_next(obj_data_t **new_object, odisk_state_t *odisk)
{
	char				path_name[NAME_MAX];
	int				err;
	gid_idx_ent_t			gid_ent;
	int				i;
	int				num;


again:
	for (i = odisk->cur_file; i < odisk->max_files; i++ ) {
		if (odisk->index_files[i] != NULL) {
			num = fread(&gid_ent, sizeof(gid_ent), 1, 
				odisk->index_files[i]);
			if (num == 1) {
				sprintf(path_name, "%s/%s", odisk->odisk_path, 
					gid_ent.gid_name);
				err = odisk_load_obj(new_object, path_name);
				if (err) {
					/* XXX log */
					printf("load obj <%s> failed %d \n", path_name, err);
					return(err);
				} else {
					odisk->cur_file = i + 1;
					if (odisk->cur_file >= odisk->max_files) {
						odisk->cur_file = 0;
					}
					return(0);
				}
			} else {
				/*
				 * This file failed, close it and continue.
				 */
				fclose(odisk->index_files[i]);	
				odisk->index_files[i] = NULL;
			}
		}
	}

	/*
	 * if we get here, either we need to start at the begining,
	 * or there is no more data.
	 */
	if (odisk->cur_file != 0) {
		odisk->cur_file = 0;
		goto again;
	} else {
		return(ENOENT);
	}

}




static void *
odisk_main(void *arg)
{
	odisk_state_t  *	ostate = (odisk_state_t *)arg;
	obj_data_t*		nobj;
	int			err;

	while (1) {
		/* If there is no search don't do anything */
		pthread_mutex_lock(&shared_mutex);
		while (search_active == 0) {
			pthread_cond_wait(&bg_active_cv, &shared_mutex);
		}
		pthread_mutex_unlock(&shared_mutex);
	
		/* get the next object */
		err = odisk_read_next(&nobj, ostate);
	
		pthread_mutex_lock(&shared_mutex);
		if (err == ENOENT) {
			search_active = 0;
			search_done = 1;
			if (fg_wait) {
				fg_wait = 0;
				pthread_cond_signal(&fg_data_cv);
			}
		} else {
			if (fg_wait) {
				fg_wait = 0;
				pthread_cond_signal(&fg_data_cv);
			}
			if (obj_q == NULL) {
				obj_q = nobj;
			} else {
				bg_wait_q  = 1;
				pthread_cond_wait(&bg_queue_cv, &shared_mutex);
				obj_q = nobj;
				if (fg_wait) {
					fg_wait = 0;
					pthread_cond_signal(&fg_data_cv);
				}
			}
		}
		pthread_mutex_unlock(&shared_mutex);
	}
}

int
odisk_next_obj(obj_data_t **new_object, odisk_state_t *odisk)
{

	pthread_mutex_lock(&shared_mutex);
	while (1) {
		if (search_done) {
			pthread_mutex_unlock(&shared_mutex);
			return(ENOENT);
		}

		if (obj_q != NULL) {
			*new_object = obj_q;
			obj_q = NULL;
			if (bg_wait_q) {
				bg_wait_q = 0;
				pthread_cond_signal(&bg_queue_cv);
			}
			pthread_mutex_unlock(&shared_mutex);
			return(0);
		} else {
			fg_wait = 1;
			pthread_cond_wait(&fg_data_cv, &shared_mutex);
		}
	} 
}


int
odisk_init(odisk_state_t **odisk, char *dir_path)
{
	odisk_state_t  *	new_state;
	int			err;

	if (strlen(dir_path) > (MAX_DIR_PATH-1)) {
		/* XXX log */
		return(EINVAL);
	}

	new_state = (odisk_state_t *)malloc(sizeof(*new_state));
	if (new_state == NULL) {
		/* XXX err log */
		return(ENOMEM);
	}

	memset(new_state, 0, sizeof(*new_state));

	/* the length has already been tested above */
	strcpy(new_state->odisk_path, dir_path);	

	err = pthread_create(&new_state->thread_id, PATTR_DEFAULT, 
		odisk_main, (void *) new_state);

	*odisk = new_state;
	return(0);
}


int
odisk_reset(odisk_state_t *odisk)
{
	char	idx_file[256];	/* XXX */
	FILE *	new_file;
	int	i;

	/*
	 * First go through all the index files and close them.
	 */
	for (i=0; i < odisk->max_files; i++) {
		if (odisk->index_files[i] != NULL) {
			fclose(odisk->index_files[i]);
			odisk->index_files[i] = NULL;
		}
	}


	for (i=0; i < odisk->num_gids; i++) {
		sprintf(idx_file, "%s/%s%016llX", odisk->odisk_path, GID_IDX, 
			odisk->gid_list[i]);
		new_file = fopen(idx_file, "r");
		if (new_file == NULL) {
			fprintf(stderr, "unable to open idx %s \n", idx_file);
		} else {
			odisk->index_files[i] = new_file;
		}
	}
	
	odisk->max_files = odisk->num_gids;
	odisk->cur_file = 0;

	pthread_mutex_lock(&shared_mutex);
	search_active = 1;
	search_done = 0;
	pthread_cond_signal(&bg_active_cv);
	pthread_mutex_unlock(&shared_mutex);

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
	if (glist->gids[i] == 0) {
		continue;
	}
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
				printf("removing: %s \n", max_path);
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
