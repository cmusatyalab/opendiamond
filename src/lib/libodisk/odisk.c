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
#include "lib_log.h"
#include "lib_dctl.h"
#include "odisk_priv.h"
#include "attr.h"
#include "rtimer.h"
#include "ring.h"


/*
 * forward declarations 
 */
static void     update_gid_idx(odisk_state_t * odisk, char *name,
                               groupid_t * gid);
static void     delete_object_gids(odisk_state_t * odisk, obj_data_t * obj);

/*
 * These are the set of group ID's we are using to 
 * filter the data.
 */
#define MAX_GID_FILTER  64

int
odisk_load_obj(obj_data_t ** obj_handle, char *name)
{
    obj_data_t     *new_obj;
    struct stat     stats;
    int             os_file;
    char           *data;
    int             err, len;
    size_t          size;
    uint64_t        local_id;
    char           *ptr;
    char            attr_name[NAME_MAX];

    if (strlen(name) >= NAME_MAX) {
        /*
         * XXX log error 
         */
        return (EINVAL);
    }

    new_obj = malloc(sizeof(*new_obj));
    if (new_obj == NULL) {
        /*
         * XXX log error 
         */
        return (ENOMEM);
    }

    /*
     * open the file 
     */
    os_file = open(name, O_RDONLY);
    if (os_file == -1) {
        free(new_obj);
        return (ENOENT);
    }

    err = fstat(os_file, &stats);
    if (err != 0) {
        free(new_obj);
        return (ENOENT);
    }


    data = (char *) malloc(stats.st_size);
    if (data == NULL) {
        close(os_file);
        free(new_obj);
        return (ENOENT);

    }

    if (stats.st_size > 0) {
        size = read(os_file, data, stats.st_size);
        if (size != stats.st_size) {
            /*
             * XXX log error 
             */
            printf("odisk_load_obj: failed to reading data \n");
            free(data);
            close(os_file);
            free(new_obj);
            return (ENOENT);
        }
    }
    new_obj->data = data;
    new_obj->data_len = stats.st_size;

    ptr = rindex(name, '/');
    if (ptr == NULL) {
        ptr = name;
    } else {
        ptr++;
    }
    sscanf(ptr, "OBJ%016llX", &local_id);
    new_obj->local_id = local_id;

    /*
     * Load the attributes, if any.
     */
    len = snprintf(attr_name, NAME_MAX, "%s%s", name, ATTR_EXT);
    assert(len < NAME_MAX);

    obj_read_attr_file(attr_name, &new_obj->attr_info);


    *obj_handle = (obj_data_t *) new_obj;

    close(os_file);

    return (0);
}

int
odisk_get_obj_cnt(odisk_state_t * odisk)
{
    int             count = 0;
    char            idx_file[NAME_MAX];
    FILE           *new_file;
    gid_idx_ent_t   gid_ent;
    int             i;
    int             len;

    for (i = 0; i < odisk->num_gids; i++) {
        len = snprintf(idx_file, NAME_MAX, "%s/%s%016llX", odisk->odisk_path,
                       GID_IDX, odisk->gid_list[i]);
        assert(len < NAME_MAX);
        new_file = fopen(idx_file, "r");
        if (new_file == NULL) {
            continue;
        }
        while (fread(&gid_ent, sizeof(gid_ent), 1, new_file) == 1) {
            count++;
        }
        fclose(new_file);
    }

    return (count);

}

int
odisk_save_obj(odisk_state_t * odisk, obj_data_t * obj)
{
    char            buf[NAME_MAX];
    char            attrbuf[NAME_MAX];
    FILE           *os_file;
    int             size;
    int             len;

    len = snprintf(buf, NAME_MAX, "%s/OBJ%016llX", odisk->odisk_path,
                   obj->local_id);
    assert(len < NAME_MAX);


    /*
     * open the file 
     */
    os_file = fopen(buf, "wb");
    if (os_file == NULL) {
        printf("Failed to open save obj \n");
        return (ENOENT);
    }

    if (obj->data_len > 0) {
        size = fwrite(obj->data, obj->data_len, 1, os_file);
        if (size != 1) {
            /*
             * XXX log error 
             */
            printf("failed to write data \n");
            return (ENOENT);
        }
    }


    len = snprintf(attrbuf, NAME_MAX, "%s%s", buf, ATTR_EXT);
    assert(len < NAME_MAX);
    obj_write_attr_file(attrbuf, &obj->attr_info);


    fclose(os_file);
    return (0);
}

int
odisk_delete_obj(odisk_state_t * odisk, obj_data_t * obj)
{
    char            buf[NAME_MAX];
    int             len;
	int				err;

    delete_object_gids(odisk, obj);

    len = snprintf(buf, NAME_MAX, "%s/OBJ%016llX", odisk->odisk_path,
                   obj->local_id);
    assert(len < NAME_MAX);
    err = unlink(buf);
	if (err == -1) {
		fprintf(stderr, "failed unlink %s:" , buf);
		perror("");
	}

    len = snprintf(buf, NAME_MAX, "%s/OBJ%016llX%s", odisk->odisk_path,
                   obj->local_id, ATTR_EXT);
    assert(len < NAME_MAX);

    err = unlink(buf);
	if (err == -1) {
		fprintf(stderr, "failed unlink %s:" , buf);
		perror("");
	}

    return (0);
}




int
odisk_get_obj(odisk_state_t * odisk, obj_data_t ** obj, obj_id_t * oid)
{
    char            buf[NAME_MAX];
    int             err;
    int             len;

    len = snprintf(buf, NAME_MAX, "%s/OBJ%016llX", odisk->odisk_path,
                   oid->local_id);
    assert(len < NAME_MAX);

    err = odisk_load_obj(obj, buf);
    (*obj)->local_id = oid->local_id;
    return (err);
}


int
odisk_release_obj(odisk_state_t * odisk, obj_data_t * obj)
{

    if (obj->data != NULL) {
        free(obj->data);
    }

    if (obj->attr_info.attr_data != NULL) {
        free(obj->attr_info.attr_data);
    }

    free(obj);
    return (0);
}


int
odisk_add_gid(odisk_state_t * odisk, obj_data_t * obj, groupid_t * gid)
{
    gid_list_t     *glist;
    off_t           len;
    int             i, err;
    int             space;

    len = 0;
    err = obj_read_attr(&obj->attr_info, GIDLIST_NAME, &len, NULL);
    if (err == ENOENT) {
        glist = (gid_list_t *) malloc(GIDLIST_SIZE(4));
        assert(glist != NULL);
        memset(glist, 0, GIDLIST_SIZE(4));
    } else if (err != ENOMEM) {
        return (err);
    } else {
        glist = (gid_list_t *) malloc(len);
        err =
            obj_read_attr(&obj->attr_info, GIDLIST_NAME, &len,
                          (char *) glist);
        assert(err == 0);
    }

    space = -1;
    for (i = 0; i < glist->num_gids; i++) {
        if ((glist->gids[i] == 0) && (space == -1)) {
            space = i;
        }
        if (glist->gids[i] == *gid) {
            return (EAGAIN);
        }
    }

    if (space == -1) {
        int             old, new;
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
                         GIDLIST_SIZE(glist->num_gids), (char *) glist);
    assert(err == 0);

    return (0);
}


int
odisk_rem_gid(odisk_state_t * odisk, obj_data_t * obj, groupid_t * gid)
{
    gid_list_t     *glist;
    off_t           len;
    int             i, err;

    len = 0;
    err = obj_read_attr(&obj->attr_info, GIDLIST_NAME, &len, NULL);
    if (err != ENOMEM) {
        return (err);
    }

    glist = (gid_list_t *) malloc(len);
    err = obj_read_attr(&obj->attr_info, GIDLIST_NAME, &len, (char *) glist);
    assert(err == 0);


    for (i = 0; i < glist->num_gids; i++) {
        if (glist->gids[i] == *gid) {
            glist->gids[i] = *gid;
            err = obj_write_attr(&obj->attr_info, GIDLIST_NAME,
                                 GIDLIST_SIZE(glist->num_gids),
                                 (char *) glist);
            return (0);
        }
    }

    return (ENOENT);
}

int
odisk_new_obj(odisk_state_t * odisk, obj_id_t * oid, groupid_t * gid)
{
    char            buf[NAME_MAX];
    uint64_t        local_id;
    int             fd;
    obj_data_t     *obj;
    int             len;

    local_id = rand();

    while (1) {
        len = snprintf(buf, NAME_MAX, "%s/OBJ%016llX", odisk->odisk_path,
                       local_id);
        assert(len < NAME_MAX);
        fd = open(buf, O_CREAT | O_EXCL, 0777);
        if (fd == -1) {
            local_id = rand();
        } else {
            break;
        }
    }

    oid->local_id = local_id;
    close(fd);

    odisk_get_obj(odisk, &obj, oid);
    odisk_add_gid(odisk, obj, gid);

    len = snprintf(buf, NAME_MAX, "OBJ%016llX", local_id);
    assert(len < NAME_MAX);

    update_gid_idx(odisk, buf, gid);

    odisk_save_obj(odisk, obj);
    odisk_release_obj(odisk, obj);

    return (0);
}


int
odisk_clear_gids(odisk_state_t * odisk)
{
    odisk->num_gids = 0;
    return (0);
}

int
odisk_set_gid(odisk_state_t * odisk, groupid_t gid)
{
    int             i;

    /*
     * make sure this GID is not already in the list 
     */
    for (i = 0; i < odisk->num_gids; i++) {
        if (odisk->gid_list[i] == gid) {
            return (0);
        }
    }

    /*
     * make sure there is room for this new entry.
     */
    if (odisk->num_gids >= MAX_GID_FILTER) {
        return (ENOMEM);
    }

    odisk->gid_list[odisk->num_gids] = gid;
    odisk->num_gids++;
    return (0);
}

int
odisk_write_obj(odisk_state_t * odisk, obj_data_t * obj, int len,
                int offset, char *data)
{
    int             total_len;
    char           *dbuf;

    total_len = offset + len;

    if (total_len > obj->data_len) {
        dbuf = realloc(obj->data, total_len);
        assert(dbuf != NULL);
        obj->data_len = total_len;
        obj->data = dbuf;
    }

    memcpy(&obj->data[offset], data, len);

    return (0);
}


int
odisk_read_obj(odisk_state_t * odisk, obj_data_t * obj, int *len,
               int offset, char *data)
{
    int             rlen;
    int             remain;

    if (offset >= obj->data_len) {
        *len = 0;
        return (0);
    }

    remain = obj->data_len - offset;
    if (remain > *len) {
        rlen = *len;
    } else {
        rlen = remain;
    }


    memcpy(data, &obj->data[offset], rlen);

    *len = rlen;

    return (0);
}

/*
 * XXX shared state , move into state descriptor ???
 */
static int      search_active = 0;
static int      search_done = 0;
static int      bg_wait_q = 0;
static int      fg_wait = 0;
static ring_data_t *obj_ring;
static pthread_mutex_t shared_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t fg_data_cv = PTHREAD_COND_INITIALIZER;
static pthread_cond_t bg_active_cv = PTHREAD_COND_INITIALIZER;
static pthread_cond_t bg_queue_cv = PTHREAD_COND_INITIALIZER;
#define	OBJ_RING_SIZE	64


int
odisk_read_next(obj_data_t ** new_object, odisk_state_t * odisk)
{
    char            path_name[NAME_MAX];
    int             err;
    gid_idx_ent_t   gid_ent;
    int             i;
    int             num;
    int             len;


  again:
    for (i = odisk->cur_file; i < odisk->max_files; i++) {
        if (odisk->index_files[i] != NULL) {
            num = fread(&gid_ent, sizeof(gid_ent), 1, odisk->index_files[i]);
            if (num == 1) {
                len = snprintf(path_name, NAME_MAX, "%s/%s",
						odisk->odisk_path, gid_ent.gid_name);
                assert(len < NAME_MAX);

                err = odisk_load_obj(new_object, path_name);

                if (err) {
                    /*
                     * if we can't load the object it probably go deleted
                     * between the time the search started (and we got the
                     * gidindex file and the time we tried to open it.  We
                     * just continue on. 
                     */
                    continue;
                } else {
                    odisk->cur_file = i + 1;
                    if (odisk->cur_file >= odisk->max_files) {
                        odisk->cur_file = 0;
                    }
                    return (0);
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
        return (ENOENT);
    }
}




static void    *
odisk_main(void *arg)
{
    odisk_state_t  *ostate = (odisk_state_t *) arg;
    obj_data_t     *nobj;
    int             err;

    dctl_thread_register(ostate->dctl_cookie);
    log_thread_register(ostate->log_cookie);

    while (1) {
        /*
         * If there is no search don't do anything 
         */
        pthread_mutex_lock(&shared_mutex);
        while (search_active == 0) {
            pthread_cond_wait(&bg_active_cv, &shared_mutex);
        }
        pthread_mutex_unlock(&shared_mutex);

        /*
         * get the next object 
         */
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
            if (!ring_full(obj_ring)) {
                ring_enq(obj_ring, nobj);
            } else {
                bg_wait_q = 1;
                pthread_cond_wait(&bg_queue_cv, &shared_mutex);
                ring_enq(obj_ring, nobj);
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
odisk_next_obj(obj_data_t ** new_object, odisk_state_t * odisk)
{

    pthread_mutex_lock(&shared_mutex);
    while (1) {
        if (search_done) {
            pthread_mutex_unlock(&shared_mutex);
            return (ENOENT);
        }


        if (!ring_empty(obj_ring)) {
            *new_object = ring_deq(obj_ring);
            if (bg_wait_q) {
                bg_wait_q = 0;
                pthread_cond_signal(&bg_queue_cv);
            }
            pthread_mutex_unlock(&shared_mutex);
            return (0);
        } else {
            fg_wait = 1;
            pthread_cond_wait(&fg_data_cv, &shared_mutex);
        }
    }
}

int
odisk_num_waiting(odisk_state_t * odisk)
{
    return (ring_count(obj_ring));
}


int
odisk_init(odisk_state_t ** odisk, char *dir_path, void *dctl_cookie,
           void *log_cookie)
{
    odisk_state_t  *new_state;
    int             err;

    if (strlen(dir_path) > (MAX_DIR_PATH - 1)) {
        /*
         * XXX log 
         */
        return (EINVAL);
    }

    ring_init(&obj_ring, OBJ_RING_SIZE);

    new_state = (odisk_state_t *) malloc(sizeof(*new_state));
    if (new_state == NULL) {
        /*
         * XXX err log 
         */
        return (ENOMEM);
    }

    memset(new_state, 0, sizeof(*new_state));

    new_state->dctl_cookie = dctl_cookie;
    new_state->log_cookie = log_cookie;

    /*
     * the length has already been tested above 
     */
    strcpy(new_state->odisk_path, dir_path);

    err = pthread_create(&new_state->thread_id, PATTR_DEFAULT,
                         odisk_main, (void *) new_state);

    *odisk = new_state;
    return (0);
}


int
odisk_reset(odisk_state_t * odisk)
{
    char            idx_file[NAME_MAX];
    FILE           *new_file;
    int             i;
    int             len;

    /*
     * First go through all the index files and close them.
     */
    for (i = 0; i < odisk->max_files; i++) {
        if (odisk->index_files[i] != NULL) {
            fclose(odisk->index_files[i]);
            odisk->index_files[i] = NULL;
        }
    }


    for (i = 0; i < odisk->num_gids; i++) {
        len = snprintf(idx_file, NAME_MAX, "%s/%s%016llX", odisk->odisk_path,
                       GID_IDX, odisk->gid_list[i]);
        assert(len < NAME_MAX);
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

    return (0);
}

int
odisk_term(odisk_state_t * odisk)
{
    int             err;


    err = closedir(odisk->odisk_dir);

    odisk->odisk_dir = NULL;

    free(odisk);
    return (err);
}

static void
update_gid_idx(odisk_state_t * odisk, char *name, groupid_t * gid)
{
    char            idx_name[NAME_MAX];
    FILE           *idx_file;
    int             num;
    int             len;
    gid_idx_ent_t   gid_idx;

    len = snprintf(idx_name, NAME_MAX, "%s/%s%016llX", odisk->odisk_path,
                   GID_IDX, *gid);
    assert(len < NAME_MAX);

    idx_file = fopen(idx_name, "a");
    if (idx_file == NULL) {
        fprintf(stderr, "update_gid_idx: failed to open <%s> \n", idx_name);
        return;
    }

    memset(&gid_idx, 0, sizeof(gid_idx));
    len = snprintf(gid_idx.gid_name, NAME_MAX, "%s", name);
    assert(len < NAME_MAX);

    num = fwrite(&gid_idx, sizeof(gid_idx), 1, idx_file);
    assert(num == 1);

    fclose(idx_file);
}

static void
remove_gid_from_idx(odisk_state_t * odisk, char *name, groupid_t * gid)
{
    char            new_name[NAME_MAX];
    char            old_name[NAME_MAX];
    FILE           *old_file;
    FILE           *new_file;
    int             num;
    gid_idx_ent_t   rem_gid_idx;
    gid_idx_ent_t   cur_gididx;
    int             err;
    int             len;

    len = snprintf(old_name, NAME_MAX, "%s/%s%016llX.old", odisk->odisk_path,
                   GID_IDX, *gid);
    assert(len < NAME_MAX);
    len = snprintf(new_name, NAME_MAX, "%s/%s%016llX", odisk->odisk_path,
                   GID_IDX, *gid);
    assert(len < NAME_MAX);


    /*
     * rename the old index file 
     */
    err = rename(new_name, old_name);
    if (err) {
        perror("Failed renaming index file:");
        return;
    }


    old_file = fopen(old_name, "r");
    if (old_file == NULL) {
        fprintf(stderr, "remove_gid_from_idx: failed to open <%s> \n",
                old_name);
        return;
    }

    new_file = fopen(new_name, "w+");
    if (new_file == NULL) {
        fprintf(stderr, "remove_gid_from_idx: failed to open <%s> \n",
                new_name);
        return;
    }

    err = unlink(old_name);
    if (err) {
        perror("removing old file ");
        /*
         * XXX what to do .... 
         */
    }

    memset(&rem_gid_idx, 0, sizeof(rem_gid_idx));
    snprintf(rem_gid_idx.gid_name, NAME_MAX, "%s", name);

    while (fread(&cur_gididx, sizeof(cur_gididx), 1, old_file) == 1) {
        if (memcmp(&cur_gididx, &rem_gid_idx, sizeof(cur_gididx)) == 0) {
            continue;
        }
        num = fwrite(&cur_gididx, sizeof(cur_gididx), 1, new_file);
        if (num != 1) {
            perror("Failed writing odisk index: ");
        }
    }

    fclose(old_file);
    fclose(new_file);
}

static void
update_object_gids(odisk_state_t * odisk, obj_data_t * obj, char *name)
{
    gid_list_t     *glist;
    off_t           len;
    int             i, err;

    len = 0;
    err = obj_read_attr(&obj->attr_info, GIDLIST_NAME, &len, NULL);
    if (err != ENOMEM) {
        /*
         * XXX log ?? 
         */
        return;
    }

    glist = (gid_list_t *) malloc(len);
    err = obj_read_attr(&obj->attr_info, GIDLIST_NAME, &len, (char *) glist);
    assert(err == 0);

    for (i = 0; i < glist->num_gids; i++) {
        if (glist->gids[i] == 0) {
            continue;
        }
        update_gid_idx(odisk, name, &glist->gids[i]);
    }

    free(glist);
}

static void
delete_object_gids(odisk_state_t * odisk, obj_data_t * obj)
{
    gid_list_t     *glist;
    off_t           len;
    int             i, err;
    char            buf[NAME_MAX];
    int             slen;

    slen = snprintf(buf, NAME_MAX, "OBJ%016llX", obj->local_id);
    assert(slen < NAME_MAX);

    len = 0;
    err = obj_read_attr(&obj->attr_info, GIDLIST_NAME, &len, NULL);
    if (err != ENOMEM) {
        /*
         * XXX log ?? 
         */
        return;
    }

    glist = (gid_list_t *) malloc(len);
    err = obj_read_attr(&obj->attr_info, GIDLIST_NAME, &len, (char *) glist);
    assert(err == 0);

    for (i = 0; i < glist->num_gids; i++) {
        if (glist->gids[i] == 0) {
            continue;
        }
        remove_gid_from_idx(odisk, buf, &glist->gids[i]);
    }

    free(glist);
}



/*
 * Clear all of the GID index files.  
 */

int
odisk_clear_indexes(odisk_state_t * odisk)
{
    struct dirent  *cur_ent;
    int             extlen, flen;
    char            idx_name[NAME_MAX];
    char           *poss_ext;
    DIR            *dir;
    int             count = 0;
    int             err, len;

    dir = opendir(odisk->odisk_path);
    if (dir == NULL) {
        /*
         * XXX log 
         */
        printf("failed to open %s \n", odisk->odisk_path);
        return (0);
    }


    while (1) {

        cur_ent = readdir(dir);
        /*
         * If readdir fails, then we have enumerated all
         * the contents.
         */

        if (cur_ent == NULL) {
            closedir(dir);
            return (count);
        }

        /*
         * If this isn't a file then we skip the entry.
         */
        if ((cur_ent->d_type != DT_REG) && (cur_ent->d_type != DT_LNK)) {
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
                len = snprintf(idx_name, NAME_MAX, "%s/%s", odisk->odisk_path,
                               cur_ent->d_name);
                assert(len < NAME_MAX);
                err = remove(idx_name);
                if (err == -1) {
                    fprintf(stderr, "Failed to remove %s\n", idx_name);
                }
            }
        }
    }

    closedir(dir);
}

int
odisk_build_indexes(odisk_state_t * odisk)
{
    struct dirent  *cur_ent;
    int             extlen, flen;
    char           *poss_ext;
    char            max_path[NAME_MAX];
    DIR            *dir;
    int             count = 0;
    int             err, len;
    obj_data_t     *new_object;

    dir = opendir(odisk->odisk_path);
    if (dir == NULL) {
        /*
         * XXX log 
         */
        printf("failed to open %s \n", odisk->odisk_path);
        return (0);
    }


    while (1) {

        cur_ent = readdir(dir);
        /*
         * If readdir fails, then we have enumerated all
         * the contents.
         */

        if (cur_ent == NULL) {
            closedir(dir);
            return (count);
        }

        /*
         * If this isn't a file then we skip the entry.
         */
        if ((cur_ent->d_type != DT_REG) && (cur_ent->d_type != DT_LNK)) {
            continue;
        }

        /*
         * make sure this isn't an index file 
         */
        flen = strlen(cur_ent->d_name);
        extlen = strlen(GID_IDX);
        if (flen > extlen) {
            if (strncmp(cur_ent->d_name, GID_IDX, extlen) == 0) {
                continue;
            }
        }

        /*
         * see if this is an attribute file 
         */
        extlen = strlen(ATTR_EXT);
        flen = strlen(cur_ent->d_name);
        if (flen > extlen) {
            poss_ext = &cur_ent->d_name[flen - extlen];
            if (strcmp(poss_ext, ATTR_EXT) == 0) {
                continue;
            }
        }

        len = snprintf(max_path, NAME_MAX, "%s/%s", odisk->odisk_path,
                       cur_ent->d_name);
        assert(len < NAME_MAX);

        err = odisk_load_obj(&new_object, max_path);
        if (err) {
            /*
             * XXX log 
             */
            fprintf(stderr, "create obj failed %d \n", err);
            return (err);
        }

        /*
         * Go through each of the GID's and update the index file.
         */

        update_object_gids(odisk, new_object, cur_ent->d_name);
        odisk_release_obj(odisk, new_object);

    }

    closedir(dir);
}


int
odisk_write_oids(odisk_state_t * odisk, uint32_t devid)
{
    struct dirent  *cur_ent;
    int             extlen, flen;
    char           *poss_ext;
    char            max_path[NAME_MAX];
    DIR            *dir;
    int             count = 0;
    int             err;
    obj_id_t        obj_id;
    obj_data_t     *new_object;
    int             len;

    obj_id.dev_id = (uint64_t) devid;

    dir = opendir(odisk->odisk_path);
    if (dir == NULL) {
        /*
         * XXX log 
         */
        printf("failed to open %s \n", odisk->odisk_path);
        return (0);
    }


    while (1) {

        cur_ent = readdir(dir);
        /*
         * If readdir fails, then we have enumerated all
         * the contents.
         */

        if (cur_ent == NULL) {
            closedir(dir);
            return (count);
        }

        /*
         * If this isn't a file then we skip the entry.
         */
        if ((cur_ent->d_type != DT_REG) && (cur_ent->d_type != DT_LNK)) {
            continue;
        }

        /*
         * make sure this isn't an index file 
         */
        flen = strlen(cur_ent->d_name);
        extlen = strlen(GID_IDX);
        if (flen > extlen) {
            if (strncmp(cur_ent->d_name, GID_IDX, extlen) == 0) {
                continue;
            }
        }

        /*
         * see if this is an attribute file 
         */
        extlen = strlen(ATTR_EXT);
        flen = strlen(cur_ent->d_name);
        if (flen > extlen) {
            poss_ext = &cur_ent->d_name[flen - extlen];
            if (strcmp(poss_ext, ATTR_EXT) == 0) {
                continue;
            }
        }

        len = snprintf(max_path, NAME_MAX, "%s/%s", odisk->odisk_path,
                       cur_ent->d_name);

        err = odisk_load_obj(&new_object, max_path);
        if (err) {
            /*
             * XXX log 
             */
            fprintf(stderr, "create obj failed %d \n", err);
            return (err);
        }
        obj_id.local_id = new_object->local_id;

        /*
         * XXX write attribute 
         */
        err = obj_write_attr(&new_object->attr_info, "MY_OID",
                             sizeof(obj_id), (char *) &obj_id);
        assert(err == 0);

        /*
         * save the state 
         */
        odisk_save_obj(odisk, new_object);
        odisk_release_obj(odisk, new_object);
    }

    closedir(dir);
}
