/*
 * These file handles a lot of the device specific code.  For the current
 * version we have state for each of the devices.
 */
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>
#include <netdb.h>
#include "ring.h"
#include "rstat.h"
#include "lib_searchlet.h"
#include "lib_od.h"
#include "lib_odisk.h"
#include "lib_search_priv.h"
#include "consts.h"
#include "log.h"
#include "log_impl.h"
#include "assert.h"
#include "lib_dctl.h"
#include "lib_hstub.h"
#include "dctl_common.h"


/*
 *  This function intializes the background processing thread that
 *  is used for taking data ariving from the storage devices
 *  and completing the processing.  This thread initializes the ring
 *  that takes incoming data.
 */

void
dev_log_data_cb(void *cookie, char *data, int len, int devid)
{

	log_info_t *		linfo;
	device_handle_t *	dev;


	dev = (device_handle_t *)cookie;

	linfo = (log_info_t *)malloc(sizeof(*linfo));
	if (linfo == NULL) {
		/* we failed to allocate the space, we just
		 * free the log data.
		 */
		free(data);
		return;
	}


	linfo->data = data;
	linfo->len = len;
	linfo->dev = devid;

	if (ring_enq(dev->sc->log_ring, (void *)linfo)) {
		/*
		 * If we failed to log, then we just fee
		 * the information and loose this log.
		 */
		free(data);
		free(linfo);
		return;	
	}

	return;
}


/* XXX  ret type??*/
int
dev_new_obj_cb(void *hcookie, obj_data_t *odata, int ver_no)
{

	device_handle_t *	dev;
	int			err;
	obj_info_t *		oinfo;
	dev = (device_handle_t *)hcookie;

	oinfo = (obj_info_t *)malloc (sizeof(*oinfo));
	if (oinfo == NULL ) {
		printf("XXX failed oinfo malloc \n");
		exit(1);
	}
	oinfo->ver_num = ver_no; /* XXX XXX */
	oinfo->obj = odata;	
	err = ring_enq(dev->sc->unproc_ring, (void *)oinfo);
	if (err) {
			/* XXX */
		printf("ring_enq failed \n");
	}
	return(0);
}

void
dev_search_done_cb(void *hcookie, int ver_no)
{

	device_handle_t *	dev;
	dev = (device_handle_t *)hcookie;

	if (dev->sc->cur_search_id != ver_no) {
		/* XXX */
		printf("search done but vno doesn't match !!! \n");
		return;
	}

	printf("search done \n");
	dev->flags |= DEV_FLAG_COMPLETE;

	return;
}


/*
 * Search through the list of current devices to see
 * if we have one with the same ID. 
 */

static device_handle_t *
lookup_dev_by_id(search_context_t *sc, uint32_t devid)
{
	device_handle_t *cur_dev;

	cur_dev = sc->dev_list;
	while(cur_dev != NULL) {
		if (cur_dev->dev_id == devid) {
			break;
		}
		cur_dev = cur_dev->next;
	}

	return(cur_dev);	
}


#define     MAX_PENDING     64

typedef struct dctl_pending {
        int                 pend_allocated;
        pthread_mutex_t     pend_mutex;
        pthread_cond_t      pend_cond;
        int *               pend_len;
        char *              pend_data;
        dctl_data_type_t *  pend_dtype;
        int                 pend_err;
} dctl_pending_t;


dctl_pending_t      pend_data[MAX_PENDING];


static int
allocate_pending()
{
    int     i;
    
    for (i=0; i < MAX_PENDING; i++) {
            pthread_mutex_lock(&pend_data[i].pend_mutex);

            if (pend_data[i].pend_allocated == 0) {
                pend_data[i].pend_allocated = 1;
                pthread_mutex_unlock(&pend_data[i].pend_mutex);
                return(i);
            } else {
                pthread_mutex_unlock(&pend_data[i].pend_mutex);
            }
    }
    return(-1);
}

static void
free_pending(int idx)
{
    pthread_mutex_lock(&pend_data[idx].pend_mutex);
    pend_data[idx].pend_allocated = 0;
    pthread_mutex_unlock(&pend_data[idx].pend_mutex);
}



static void
init_pending()
{
    int     i;
    
    for (i=0; i < MAX_PENDING; i++) {
            pthread_cond_init(&pend_data[i].pend_cond, NULL);
            pthread_mutex_init(&pend_data[i].pend_mutex, NULL);
            pend_data[i].pend_allocated = 0;
    }

}



static void
write_leaf_done_cb(void *cookie, int err, int32_t opid)
{
    int idx;
    

    idx = (int)opid;

    assert(idx >= 0);
    assert(idx < MAX_PENDING);


    pthread_mutex_lock(&pend_data[idx].pend_mutex);

    pend_data[idx].pend_err = err;

    pthread_cond_signal(&pend_data[idx].pend_cond);
    pthread_mutex_unlock(&pend_data[idx].pend_mutex);

}


static int
remote_write_leaf(char *path, int len, char *data, void *cookie)
{
	device_handle_t *cur_dev;
    int             idx;
    int             err;

    cur_dev = (device_handle_t *)cookie;

    idx = allocate_pending();
    if (idx < 0) {
            return(ENOSPC);
    }
            
    pthread_mutex_lock(&pend_data[idx].pend_mutex);

    /* we don't care about keeping the data of len here */
    err = device_write_leaf(cur_dev->dev_handle, path, len, data, idx);
    if (err) {
        pthread_mutex_unlock(&pend_data[idx].pend_mutex);
        free_pending(idx);
        return(err);
    }


    pthread_cond_wait(&pend_data[idx].pend_cond, &pend_data[idx].pend_mutex);

    /* get error from the request and save it */
    err = pend_data[idx].pend_err;

    pthread_mutex_unlock(&pend_data[idx].pend_mutex);
    free_pending(idx);

    return(err);
}



static void
read_leaf_done_cb(void *cookie, int err, dctl_data_type_t dtype, 
                int len, char *data, int32_t opid)
{
    int     idx;

    idx = (int)opid;

    assert(idx >= 0);
    assert(idx < MAX_PENDING);


    pthread_mutex_lock(&pend_data[idx].pend_mutex);

    if (err) {
        pend_data[idx].pend_err = err;

    } else {
        if (*pend_data[idx].pend_len < len) {
            pend_data[idx].pend_err = ENOMEM;
            *pend_data[idx].pend_len = len;

        } else {
            pend_data[idx].pend_err = 0;
            *pend_data[idx].pend_len = len;
            *pend_data[idx].pend_dtype = dtype;
            memcpy(pend_data[idx].pend_data, data, len);
        }
    }

    pthread_cond_signal(&pend_data[idx].pend_cond);
    pthread_mutex_unlock(&pend_data[idx].pend_mutex);
}


static int
remote_read_leaf(char *path, dctl_data_type_t *dtype, int *len, 
                char *data, void *cookie)
{
	device_handle_t *cur_dev;
    int             idx;
    int             err;

    cur_dev = (device_handle_t *)cookie;

    idx = allocate_pending();
    if (idx < 0) {
            return(ENOSPC);
    }
            
    pthread_mutex_lock(&pend_data[idx].pend_mutex);

    pend_data[idx].pend_len = len;
    pend_data[idx].pend_data = data; 
    pend_data[idx].pend_dtype = dtype; 
    pthread_mutex_unlock(&pend_data[idx].pend_mutex);

    /* we don't care about keeping the data of len here */
    err = device_read_leaf(cur_dev->dev_handle, path, idx);
    if (err) {
        free_pending(idx);
        return(err);
    }

    pthread_mutex_lock(&pend_data[idx].pend_mutex);
    pthread_cond_wait(&pend_data[idx].pend_cond, &pend_data[idx].pend_mutex);

    /* get error from the request and save it */
    err = pend_data[idx].pend_err;

    pthread_mutex_unlock(&pend_data[idx].pend_mutex);
    free_pending(idx);

    return(err);
}


static void
lnodes_done_cb(void *cookie, int err, int num_ents, dctl_entry_t *data, 
                int32_t opid)
{
    int             idx;

    idx = (int)opid;

    assert(idx >= 0);
    assert(idx < MAX_PENDING);

    pthread_mutex_lock(&pend_data[idx].pend_mutex);

    if (err) {
        pend_data[idx].pend_err = err;

    } else {
        if (*pend_data[idx].pend_len < num_ents) {
            pend_data[idx].pend_err = ENOMEM;
            *pend_data[idx].pend_len = num_ents;

        } else {
            pend_data[idx].pend_err = 0;
            *pend_data[idx].pend_len = num_ents;
            memcpy((char *)pend_data[idx].pend_data, (char *)data, 
                            (num_ents * sizeof(dctl_entry_t)));
        }
    }

    pthread_cond_signal(&pend_data[idx].pend_cond);
    pthread_mutex_unlock(&pend_data[idx].pend_mutex);
}

static int
remote_list_nodes(char *path, int *num_ents, dctl_entry_t *space,
                                void *cookie)
{
	device_handle_t *cur_dev;
    int             idx;
    int             err;

    cur_dev = (device_handle_t *)cookie;

    idx = allocate_pending();
    if (idx < 0) {
            return(ENOSPC);
    }
    
    pthread_mutex_lock(&pend_data[idx].pend_mutex);
    pend_data[idx].pend_len = num_ents;
    pend_data[idx].pend_data = (char *)space; 
    pthread_mutex_unlock(&pend_data[idx].pend_mutex);


    err = device_list_nodes(cur_dev->dev_handle, path, idx);
    if (err) {
        free_pending(idx);
        return(err);
    }


    pthread_mutex_lock(&pend_data[idx].pend_mutex);
    pthread_cond_wait(&pend_data[idx].pend_cond, &pend_data[idx].pend_mutex);

    /* get error from the request and save it */
    err = pend_data[idx].pend_err;

    pthread_mutex_unlock(&pend_data[idx].pend_mutex);
    free_pending(idx);

    return(err);
}


static void
lleafs_done_cb(void *cookie, int err, int num_ents, dctl_entry_t *data, 
                int32_t opid)
{
    int             idx;

    idx = (int)opid;

    assert(idx >= 0);
    assert(idx < MAX_PENDING);

    pthread_mutex_lock(&pend_data[idx].pend_mutex);

    if (err) {
        pend_data[idx].pend_err = err;

    } else {
        if (*pend_data[idx].pend_len < num_ents) {
            pend_data[idx].pend_err = ENOMEM;
            *pend_data[idx].pend_len = num_ents;

        } else {
            pend_data[idx].pend_err = 0;
            *pend_data[idx].pend_len = num_ents;
            memcpy((char *)pend_data[idx].pend_data, (char *)data, 
                            (num_ents * sizeof(dctl_entry_t)));
        }
    }

    pthread_cond_signal(&pend_data[idx].pend_cond);
    pthread_mutex_unlock(&pend_data[idx].pend_mutex);
}



static int
remote_list_leafs(char *path, int *num_ents, dctl_entry_t *space,
                                void *cookie)
{
	device_handle_t *cur_dev;
    int             idx;
    int             err;

    cur_dev = (device_handle_t *)cookie;

    idx = allocate_pending();
    if (idx < 0) {
            return(ENOSPC);
    }
    
    pthread_mutex_lock(&pend_data[idx].pend_mutex);
    pend_data[idx].pend_len = num_ents;
    pend_data[idx].pend_data = (char *)space; 
    pthread_mutex_unlock(&pend_data[idx].pend_mutex);

    err = device_list_leafs(cur_dev->dev_handle, path, idx);
    if (err) {
        free_pending(idx);
        return(err);
    }


    pthread_mutex_lock(&pend_data[idx].pend_mutex);
    pthread_cond_wait(&pend_data[idx].pend_cond, &pend_data[idx].pend_mutex);

    /* get error from the request and save it */
    err = pend_data[idx].pend_err;

    pthread_mutex_unlock(&pend_data[idx].pend_mutex);
    free_pending(idx);

    return(err);
}





static void
register_remote_dctl(uint32_t devid, device_handle_t *dev_handle)
{

    struct hostent *hent;
    dctl_fwd_cbs_t  cbs;
    int             len, err;
    char *          delim;
    char            node_name[128];

    hent = gethostbyaddr(&devid, sizeof(devid), AF_INET);
    if (hent == NULL) {
        struct in_addr in;

        printf("failed to get hostname\n");
        in.s_addr = devid;
        delim = inet_ntoa(in);
        strcpy(node_name, delim);

        /* replace all the '.' with '_' */
        while ((delim = index(node_name, '.')) != NULL) {
            *delim = '_';
        }
    } else {
        delim = index(hent->h_name ,'.');
        if (delim == NULL) {
            len = strlen(hent->h_name);
        } else {
            len = delim - hent->h_name;
        }
        strncpy(node_name, hent->h_name , len);
        node_name[len] = 0;
    }

    cbs.dfwd_rleaf_cb = remote_read_leaf;
    cbs.dfwd_wleaf_cb = remote_write_leaf;
    cbs.dfwd_lnodes_cb = remote_list_nodes;
    cbs.dfwd_lleafs_cb = remote_list_leafs;
    cbs.dfwd_cookie = (void *)dev_handle;

    err = dctl_register_fwd_node(HOST_DEVICE_PATH, node_name, &cbs);
    if (err) {
        printf("XXX failed to register on %d \n", err);
        assert(0);
    }
}



/*
 * This is called called to initialize a new device
 * so that we can connect to it.
 */

static device_handle_t *
create_new_device(search_context_t *sc, uint32_t devid)
{
	device_handle_t *   new_dev;
	hstub_cb_args_t	    cb_data;
    static int          done_init = 0;

    if (!done_init) {
        init_pending();
    }

	new_dev = (device_handle_t *)malloc(sizeof(*new_dev));
	if (new_dev == NULL) {
		/* XXX log */
		printf("XXX to create new device \n"); 
		return(NULL);
	}

	new_dev->flags = 0;
	new_dev->sc = sc;
	new_dev->dev_id = devid;
	new_dev->num_groups = 0;

	cb_data.new_obj_cb = dev_new_obj_cb;
	cb_data.log_data_cb  = dev_log_data_cb;
	cb_data.search_done_cb  = dev_search_done_cb;
	cb_data.rleaf_done_cb  = read_leaf_done_cb;
	cb_data.wleaf_done_cb  = write_leaf_done_cb;
	cb_data.lnode_done_cb  = lnodes_done_cb;
	cb_data.lleaf_done_cb  = lleafs_done_cb;


	new_dev->dev_handle = device_init(sc->cur_search_id, devid, 
			(void *)new_dev, &cb_data);
	if (new_dev->dev_handle == NULL) {
		/* XXX log */
		printf("device init failed \n");
		free(new_dev);
		return (NULL);
	}

	/*
	 * Put this device on the list of devices involved
	 * in the search.
	 */
	new_dev->next = sc->dev_list;
	sc->dev_list = new_dev;

    register_remote_dctl(devid, new_dev);

	/* XXX log */

	return(new_dev);
}




int
device_add_gid(search_context_t *sc, groupid_t gid, uint32_t devid)
{

	device_handle_t * 	cur_dev;

	cur_dev = lookup_dev_by_id(sc, devid);
	if (cur_dev == NULL) {
		cur_dev = create_new_device(sc, devid);
		if (cur_dev == NULL) {
			/* XXX log */
			return(ENOENT);
		}
	}
	/*
	 * check to see if we can add more groups, if so add it to the list
	 */
	if (cur_dev->num_groups >= MAX_DEV_GROUPS) {
		fprintf(stderr, "Too many group id's \n");
		/* XXX log */
		return(ENOENT);
	}

    	device_new_gid(cur_dev->dev_handle, sc->cur_search_id, gid);

	cur_dev->dev_groups[cur_dev->num_groups] = gid;
	cur_dev->num_groups++;
	return(0);
}


