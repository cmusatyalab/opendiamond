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
#include <stdint.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <string.h>
#include <dirent.h>
#include <assert.h>
#include "ring.h"
#include "rstat.h"
#include "lib_searchlet.h"
#include "obj_attr.h"
#include "lib_od.h"
#include "lib_odisk.h"
#include "lib_dctl.h"
#include "lib_sstub.h"
#include "lib_log.h"
#include "filter_exec.h"
#include "filter_priv.h"        /* to read stats -RW */
#include "idiskd_ops.h"
#include "dctl_common.h"


/*
 * XXX other place 
 */
extern int      do_cleanup;

/*
 * XXX move to seperate header file !!! 
 */
#define	CONTROL_RING_SIZE	512

static int      search_free_obj(obj_data_t * obj);

typedef enum {
    DEV_STOP,
    DEV_TERM,
    DEV_START,
    DEV_SEARCHLET,
    DEV_BLOB
} dev_op_type_t;


typedef struct {
    char           *filter;
    char           *spec;
} dev_slet_data_t;

typedef struct {
    char           *fname;
    void           *blob;
    int             blen;
} dev_blob_data_t;

typedef struct {
    dev_op_type_t   cmd;
    int             id;
    union {
        dev_slet_data_t sdata;
        dev_blob_data_t bdata;
    } extra_data;
} dev_cmd_data_t;

extern char    *data_dir;

/*
 * XXX clean this up later 
 */

int             cpu_split = 0;
int             cpu_split_thresh = RAND_MAX;

int
search_stop(void *app_cookie, int gen_num)
{
    dev_cmd_data_t *cmd;
    search_state_t *sstate;
    int             err;

    sstate = (search_state_t *) app_cookie;

    cmd = (dev_cmd_data_t *) malloc(sizeof(*cmd));
    if (cmd == NULL) {
        return (1);
    }

    cmd->cmd = DEV_STOP;
    cmd->id = gen_num;

    err = ring_enq(sstate->control_ops, (void *) cmd);
    if (err) {
        free(cmd);
        return (1);
    }
    return (0);
}


int
search_term(void *app_cookie, int id)
{
    dev_cmd_data_t *cmd;
    search_state_t *sstate;
    int             err;

    sstate = (search_state_t *) app_cookie;

    /*
     * Allocate a new command and put it onto the ring
     * of commands being processed.
     */
    cmd = (dev_cmd_data_t *) malloc(sizeof(*cmd));
    if (cmd == NULL) {
        return (1);
    }
    cmd->cmd = DEV_TERM;
    cmd->id = id;

    /*
     * Put it on the ring.
     */
    err = ring_enq(sstate->control_ops, (void *) cmd);
    if (err) {
        free(cmd);
        return (1);
    }
    return (0);
}

int
search_setlog(void *app_cookie, uint32_t level, uint32_t src)
{
    uint32_t        hlevel,
                    hsrc;

    hlevel = ntohl(level);
    hsrc = ntohl(src);

    log_setlevel(hlevel);
    log_settype(hsrc);
}



int
search_start(void *app_cookie, int id)
{
    dev_cmd_data_t *cmd;
    int             err;
    search_state_t *sstate;

    /*
     * XXX start 
     */

    sstate = (search_state_t *) app_cookie;
    cmd = (dev_cmd_data_t *) malloc(sizeof(*cmd));
    if (cmd == NULL) {
        return (1);
    }

    cmd->cmd = DEV_START;
    cmd->id = id;


    err = ring_enq(sstate->control_ops, (void *) cmd);
    if (err) {
        free(cmd);
        return (1);
    }

    return (0);
}


/*
 * This is called to set the searchlet for the current search.
 */

int
search_set_searchlet(void *app_cookie, int id, char *filter, char *spec)
{
    dev_cmd_data_t *cmd;
    int             err;
    search_state_t *sstate;

    sstate = (search_state_t *) app_cookie;

    cmd = (dev_cmd_data_t *) malloc(sizeof(*cmd));
    if (cmd == NULL) {
        return (1);
    }

    cmd->cmd = DEV_SEARCHLET;
    cmd->id = id;

    cmd->extra_data.sdata.filter = filter;
    cmd->extra_data.sdata.spec = spec;

    err = ring_enq(sstate->control_ops, (void *) cmd);
    if (err) {
        free(cmd);
        return (1);
    }
    return (0);
}


/*
 * Reset the statistics for the current search.
 */
void
clear_ss_stats(search_state_t * sstate)
{
    sstate->obj_total = 0;
    sstate->obj_processed = 0;
    sstate->obj_dropped = 0;
    sstate->obj_passed = 0;
    sstate->obj_skipped = 0;
}





/*
 * Take the current command and process it.  Note, the command
 * will be free'd by the caller.
 */
static void
dev_process_cmd(search_state_t * sstate, dev_cmd_data_t * cmd)
{
    int             err;
    char           *obj_name;
    char           *spec_name;



    switch (cmd->cmd) {
    case DEV_STOP:
        /*
         * Stop the current search by 
         *
         */
        sstate->flags &= ~DEV_FLAG_RUNNING;

        /*
         * clean up the filter exec state 
         */
        fexec_term_search(sstate->fdata);

        /*
         * flush objects in the transmit queue 
         */
        err = sstub_flush_objs(sstate->comm_cookie, sstate->ver_no);
        assert(err == 0);

        break;

    case DEV_TERM:
        break;

    case DEV_START:
        /*
         * Start the emulated device for now.
         * XXX do this for real later.
         */

        /*
         * Clear the stats.
         */
        clear_ss_stats(sstate);

        err = odisk_reset(sstate->ostate);
        if (err) {
            /*
             * XXX log 
             */
            /*
             * XXX crap !! 
             */
            return;
        }

        /*
         * init the filter exec code 
         */
        fexec_init_search(sstate->fdata);

        sstate->obj_total = odisk_get_obj_cnt(sstate->ostate);
        sstate->ver_no = cmd->id;
        sstate->flags |= DEV_FLAG_RUNNING;
        break;

    case DEV_SEARCHLET:
        sstate->ver_no = cmd->id;

        obj_name = cmd->extra_data.sdata.filter;
        spec_name = cmd->extra_data.sdata.spec;

        err = fexec_load_searchlet(obj_name, spec_name, &sstate->fdata);

        if (err) {
            /*
             * XXX log 
             */
            assert(0);
            return;
        }

        /*
         * Remove the files that held the data.  If do_cleanup is
         * not set then we keep the files so we can do debugging.
         */
        if (do_cleanup) {
            err = unlink(obj_name);
            if (err) {
                perror("failed to unlink");
                exit(1);
            }
            unlink(spec_name);
            if (err) {
                perror("failed to unlink");
                exit(1);
            }
        }
        free(obj_name);
        free(spec_name);

        break;

    case DEV_BLOB:{
            char           *name;
            int             blen;
            void           *blob;

            name = cmd->extra_data.bdata.fname;
            blen = cmd->extra_data.bdata.blen;
            blob = cmd->extra_data.bdata.blob;

            err = fexec_set_blob(sstate->fdata, name, blen, blob);
            assert(err == 0);

            free(name);
            free(blob);
            break;
        }


    default:
        printf("unknown command %d \n", cmd->cmd);
        break;

    }
}



/*
 * XXX hack 
 */
obj_data_t     *
create_null_obj()
{
    obj_data_t     *new_obj;

    new_obj = (obj_data_t *) malloc(sizeof(*new_obj));
    assert(new_obj != NULL);

    new_obj->data_len = 0;
    new_obj->data = NULL;
    new_obj->attr_info.attr_len = 0;
    new_obj->attr_info.attr_data = NULL;

    return (new_obj);
}

/*
 * This is the main thread that executes a "search" on a device.
 * This interates it handles incoming messages as well as processing
 * object.
 */

static void    *
device_main(void *arg)
{
    search_state_t *sstate;
    dev_cmd_data_t *cmd;
    obj_data_t     *new_obj;
    int             err;
    int             any;
    struct timespec timeout;


    sstate = (search_state_t *) arg;

    log_thread_register(sstate->log_cookie);
    dctl_thread_register(sstate->dctl_cookie);

    /*
     * XXX need to open comm channel with device
     */


    while (1) {
        any = 0;
        cmd = (dev_cmd_data_t *) ring_deq(sstate->control_ops);
        if (cmd != NULL) {
            any = 1;
            dev_process_cmd(sstate, cmd);
            free(cmd);
        }

        /*
         * XXX look for data from device to process.
         */
        if ((sstate->flags & DEV_FLAG_RUNNING) &&
            (sstate->pend_objs < sstate->pend_thresh)) {
            err = odisk_next_obj(&new_obj, sstate->ostate);
            if (err == ENOENT) {
                /*
                 * XXX fexec_dump_prob(sstate->fdata); 
                 */
                /*
                 * We have processed all the objects,
                 * clear the running and set the complete
                 * flags.
                 */
                sstate->flags &= ~DEV_FLAG_RUNNING;
                sstate->flags |= DEV_FLAG_COMPLETE;

                /*
                 * XXX big hack for now.  To indiate
                 * we are done we send object with
                 * no data or attributes.
                 */
                new_obj = create_null_obj();
                err = sstub_send_obj(sstate->comm_cookie,
                                     new_obj, sstate->ver_no);
                if (err) {
                    /*
                     * XXX overflow gracefully 
                     */
                    /*
                     * XXX log 
                     */

                } else {
                    /*
                     * XXX log 
                     */
                    sstate->pend_objs++;
                }
            } else if (err) {
                /*
                 * printf("dmain: failed to get obj !! \n"); 
                 */
                /*
                 * sleep(1); 
                 */
                continue;
            } else {
                any = 1;

                if ((sstate->bp_feedback) &&
                    (sstate->pend_objs < sstate->bp_thresh)) {
                    sstate->obj_skipped++;
                    err = sstub_send_obj(sstate->comm_cookie, new_obj,
                                         sstate->ver_no);
                    if (err) {
                        /*
                         * XXX overflow gracefully 
                         */
                        /*
                         * XXX log 
                         */

                    } else {
                        /*
                         * XXX log 
                         */
                        sstate->pend_objs++;
                    }
                } else if ((cpu_split) && (random() > cpu_split_thresh)) {
                    sstate->obj_skipped++;
                    err = sstub_send_obj(sstate->comm_cookie, new_obj,
                                         sstate->ver_no);
                    if (err) {
                        /*
                         * XXX overflow gracefully 
                         */
                        /*
                         * XXX log 
                         */

                    } else {
                        /*
                         * XXX log 
                         */
                        sstate->pend_objs++;
                    }
                } else {
                    /*
                     * XXX process the object 
                     */
                    sstate->obj_processed++;

                    err = eval_filters(new_obj, sstate->fdata, 0, NULL, NULL);
                    if (err == 0) {
                        sstate->obj_dropped++;
                        search_free_obj(new_obj);

                    } else {
                        sstate->obj_passed++;
                        err = sstub_send_obj(sstate->comm_cookie, new_obj,
                                             sstate->ver_no);
                        if (err) {
                            /*
                             * XXX overflow gracefully 
                             */
                            /*
                             * XXX log 
                             */

                        } else {
                            /*
                             * XXX log 
                             */
                            sstate->pend_objs++;
                        }
                    }
                }
            }
        }

        /*
         * If we didn't have any work to process this time around,
         * then we sleep on a cond variable for a small amount
         * of time.
         */
        /*
         * XXX move mutex's to the state data structure 
         */
        if (!any) {
            timeout.tv_sec = 0;
            timeout.tv_nsec = 10000000; /* XXX 10ms */
            nanosleep(&timeout, NULL);
        }
    }
}



/*
 * This is called when we have finished sending a log entry.  For the 
 * time being, we ignore the arguments because we only have one
 * request outstanding.  This sets a condition value to wake up
 * the main thread.
 */
int
search_log_done(void *app_cookie, char *buf, int len)
{
    search_state_t *sstate;

    sstate = (search_state_t *) app_cookie;

    pthread_mutex_lock(&sstate->log_mutex);
    pthread_cond_signal(&sstate->log_cond);
    pthread_mutex_unlock(&sstate->log_mutex);

    return (0);
}


static void    *
log_main(void *arg)
{
    search_state_t *sstate;
    char           *log_buf;
    int             err;
    struct timeval  now;
    struct timespec timeout;
    struct timezone tz;
    int             len;

    tz.tz_minuteswest = 0;
    tz.tz_dsttime = 0;

    sstate = (search_state_t *) arg;
    log_thread_register(sstate->log_cookie);
    dctl_thread_register(sstate->dctl_cookie);

    while (1) {

        len = log_getbuf(&log_buf);
        if (len > 0) {

            /*
             * send the buffer 
             */
            err = sstub_send_log(sstate->comm_cookie, log_buf, len);
            if (err) {
                /*
                 * probably shouldn't happen
                 * but we ignore and return the data
                 */
                log_advbuf(len);
                continue;
            }

            /*
             * wait on cv for the send to complete 
             */
            pthread_mutex_lock(&sstate->log_mutex);
            pthread_cond_wait(&sstate->log_cond, &sstate->log_mutex);
            pthread_mutex_unlock(&sstate->log_mutex);

            /*
             * let the log library know this space can
             * be re-used.
             */
            log_advbuf(len);

        } else {
            gettimeofday(&now, &tz);
            pthread_mutex_lock(&sstate->log_mutex);
            timeout.tv_sec = now.tv_sec + 1;
            timeout.tv_nsec = now.tv_usec * 1000;

            pthread_cond_timedwait(&sstate->log_cond,
                                   &sstate->log_mutex, &timeout);
            pthread_mutex_unlock(&sstate->log_mutex);
        }
    }
}





/*
 * This is the callback that is called when a new connection
 * has been established at the network layer.  This creates
 * new search contect and creates a thread to process
 * the data. 
 */
int
search_new_conn(void *comm_cookie, void **app_cookie)
{
    search_state_t *sstate;
    int             err;

    sstate = (search_state_t *) malloc(sizeof(*sstate));
    if (sstate == NULL) {
        *app_cookie = NULL;
        return (ENOMEM);
    }

    memset((void *) sstate, 0, sizeof(*sstate));
    /*
     * Set the return values to this "handle".
     */
    *app_cookie = sstate;

    /*
     * This is called in the new process, now we initializes it
     * log data.
     */

    log_init(&sstate->log_cookie);
    dctl_init(&sstate->dctl_cookie);

    dctl_register_node(ROOT_PATH, SEARCH_NAME);

    dctl_register_leaf(DEV_SEARCH_PATH, "version_num",
                       DCTL_DT_UINT32, dctl_read_uint32, NULL,
                       &sstate->ver_no);
    dctl_register_leaf(DEV_SEARCH_PATH, "obj_total", DCTL_DT_UINT32,
                       dctl_read_uint32, NULL, &sstate->obj_total);
    dctl_register_leaf(DEV_SEARCH_PATH, "obj_processed", DCTL_DT_UINT32,
                       dctl_read_uint32, NULL, &sstate->obj_processed);
    dctl_register_leaf(DEV_SEARCH_PATH, "obj_dropped", DCTL_DT_UINT32,
                       dctl_read_uint32, NULL, &sstate->obj_dropped);
    dctl_register_leaf(DEV_SEARCH_PATH, "obj_pass", DCTL_DT_UINT32,
                       dctl_read_uint32, NULL, &sstate->obj_passed);
    dctl_register_leaf(DEV_SEARCH_PATH, "obj_skipped", DCTL_DT_UINT32,
                       dctl_read_uint32, NULL, &sstate->obj_skipped);
    dctl_register_leaf(DEV_SEARCH_PATH, "pend_objs", DCTL_DT_UINT32,
                       dctl_read_uint32, NULL, &sstate->pend_objs);
    dctl_register_leaf(DEV_SEARCH_PATH, "pend_thresh", DCTL_DT_UINT32,
                       dctl_read_uint32, dctl_write_uint32,
                       &sstate->pend_thresh);
    dctl_register_leaf(DEV_SEARCH_PATH, "bp_feedback", DCTL_DT_UINT32,
                       dctl_read_uint32, dctl_write_uint32,
                       &sstate->bp_feedback);
    dctl_register_leaf(DEV_SEARCH_PATH, "bp_thresh", DCTL_DT_UINT32,
                       dctl_read_uint32, dctl_write_uint32,
                       &sstate->bp_feedback);


    dctl_register_node(ROOT_PATH, DEV_NETWORK_NODE);

    dctl_register_node(ROOT_PATH, DEV_FEXEC_NODE);


    /*
     * initialize libfilterexec
     */
    fexec_system_init();

    /*
     * init the ring to hold the queue of pending operations.
     */
    err = ring_init(&sstate->control_ops, CONTROL_RING_SIZE);
    if (err) {
        free(sstate);
        *app_cookie = NULL;
        return (ENOENT);
    }

    sstate->flags = 0;
    sstate->comm_cookie = comm_cookie;

    sstate->pend_thresh = SSTATE_DEFAULT_OBJ_THRESH;
    sstate->pend_objs = 0;

    sstate->bp_feedback = 0;
    sstate->bp_thresh = SSTATE_DEFAULT_BP_THRESH;
    /*
     * Create a new thread that handles the searches for this current
     * search.  (We probably want to make this a seperate process ??).
     */

    err = pthread_create(&sstate->thread_id, PATTR_DEFAULT, device_main,
                         (void *) sstate);
    if (err) {
        /*
         * XXX log 
         */
        free(sstate);
        *app_cookie = NULL;
        return (ENOENT);
    }

    /*
     * Now we also setup a thread that handles getting the log
     * data and pusshing it to the host.
     */
    pthread_cond_init(&sstate->log_cond, NULL);
    pthread_mutex_init(&sstate->log_mutex, NULL);
    err = pthread_create(&sstate->log_thread, PATTR_DEFAULT, log_main,
                         (void *) sstate);
    if (err) {
        /*
         * XXX log 
         */
        free(sstate);
        /*
         * XXX what else 
         */
        return (ENOENT);
    }

    /*
     * Initialize our communications with the object
     * disk sub-system.
     */
    err = odisk_init(&sstate->ostate, data_dir, sstate->dctl_cookie,
                     sstate->log_cookie);
    if (err) {
        fprintf(stderr, "Failed to init the object disk \n");
        assert(0);
        return (err);
    }

    return (0);
}





/*
 * a request to get the characteristics of the device.
 */
int
search_get_char(void *app_cookie, int gen_num)
{
    device_char_t   dev_char;
    search_state_t *sstate;
    u_int64_t       val;
    int             err;


    sstate = (search_state_t *) app_cookie;

	/* XXX FIX */
    dev_char.dc_isa = DEV_ISA_IA32;
    dev_char.dc_speed = (r_cpu_freq(&val) ? 0 : val);
    dev_char.dc_mem = (r_freemem(&val) ? 0 : val);

    /*
     * XXX 
     */
    err = sstub_send_dev_char(sstate->comm_cookie, &dev_char);

    return 0;
}



int
search_close_conn(void *app_cookie)
{
    return (0);
}

int
search_free_obj(obj_data_t * obj)
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

/*
 * This releases an object that is no longer needed.
 */

int
search_release_obj(void *app_cookie, obj_data_t * obj)
{
    search_state_t *sstate;
    sstate = (search_state_t *) app_cookie;


    sstate->pend_objs--;

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
search_set_list(void *app_cookie, int gen_num)
{
    /*
     * printf("XXX set list \n"); 
     */
    return (0);
}

/*
 * Get the current statistics on the system.
 */

void
search_get_stats(void *app_cookie, int gen_num)
{
    search_state_t *sstate;
    dev_stats_t    *stats;
    int             err;
    int             num_filt;
    int             len;

    sstate = (search_state_t *) app_cookie;

    /*
     * Figure out how many filters we have an allocate
     * the needed space.
     */
    num_filt = fexec_num_filters(sstate->fdata);
    len = DEV_STATS_SIZE(num_filt);

    stats = (dev_stats_t *) malloc(len);
    if (stats == NULL) {
        /*
         * This is a periodic poll, so we can ingore this
         * one if we don't have enough state.
         */
        log_message(LOGT_DISK, LOGL_ERR, "search_get_stats: no mem");
        return;
    }

    /*
     * Fill in the state we can handle here.
     */
    stats->ds_objs_total = sstate->obj_total;
    stats->ds_objs_processed = sstate->obj_processed;
    stats->ds_objs_dropped = sstate->obj_dropped;
    stats->ds_objs_nproc = sstate->obj_skipped;
    stats->ds_system_load = (int) (fexec_get_load(sstate->fdata) * 100.0);  /* XXX 
                                                                             */
    stats->ds_avg_obj_time = 0;
    stats->ds_num_filters = num_filt;


    /*
     * Get the stats for each filter.
     */
    err = fexec_get_stats(sstate->fdata, num_filt, stats->ds_filter_stats);
    if (err) {
        free(stats);
        log_message(LOGT_DISK, LOGL_ERR,
                    "search_get_stats: failed to get filter stats");
        return;
    }



    /*
     * Send the stats.
     */
    err = sstub_send_stats(sstate->comm_cookie, stats, len);
    free(stats);
    if (err) {
        log_message(LOGT_DISK, LOGL_ERR,
                    "search_get_stats: failed to send stats");
        return;
    }

    return;
}

#define MAX_DBUF    1024
#define MAX_ENTS    512

int
search_read_leaf(void *app_cookie, char *path, int32_t opid)
{

    /*
     * XXX hack for now 
     */
    int             len;
    char            data_buf[MAX_DBUF];
    dctl_data_type_t dtype;
    int             err,
                    eno;
    search_state_t *sstate;

    sstate = (search_state_t *) app_cookie;

    len = MAX_DBUF;
    err = dctl_read_leaf(path, &dtype, &len, data_buf);
    /*
     * XXX deal with ENOSPC 
     */

    if (err) {
        len = 0;
    }

    eno = sstub_rleaf_response(sstate->comm_cookie, err, dtype, len,
                               data_buf, opid);
    assert(eno == 0);

    return (0);
}


int
search_write_leaf(void *app_cookie, char *path, int len, char *data,
                  int32_t opid)
{
    /*
     * XXX hack for now 
     */
    int             err,
                    eno;
    search_state_t *sstate;
    sstate = (search_state_t *) app_cookie;

    err = dctl_write_leaf(path, len, data);
    /*
     * XXX deal with ENOSPC 
     */

    if (err) {
        len = 0;
    }

    eno = sstub_wleaf_response(sstate->comm_cookie, err, opid);
    assert(eno == 0);

    return (0);
}

int
search_list_leafs(void *app_cookie, char *path, int32_t opid)
{

    /*
     * XXX hack for now 
     */
    int             err,
                    eno;
    dctl_entry_t    ent_data[MAX_ENTS];
    int             num_ents;
    search_state_t *sstate;

    sstate = (search_state_t *) app_cookie;

    num_ents = MAX_ENTS;
    err = dctl_list_leafs(path, &num_ents, ent_data);
    /*
     * XXX deal with ENOSPC 
     */

    if (err) {
        num_ents = 0;
    }

    eno = sstub_lleaf_response(sstate->comm_cookie, err, num_ents,
                               ent_data, opid);
    assert(eno == 0);

    return (0);
}


int
search_list_nodes(void *app_cookie, char *path, int32_t opid)
{

    /*
     * XXX hack for now 
     */
    int             err,
                    eno;
    dctl_entry_t    ent_data[MAX_ENTS];
    int             num_ents;
    search_state_t *sstate;

    sstate = (search_state_t *) app_cookie;

    num_ents = MAX_ENTS;
    err = dctl_list_nodes(path, &num_ents, ent_data);
    /*
     * XXX deal with ENOSPC 
     */

    if (err) {
        num_ents = 0;
    }

    eno = sstub_lnode_response(sstate->comm_cookie, err, num_ents,
                               ent_data, opid);
    assert(eno == 0);

    return (0);
}

int
search_set_gid(void *app_cookie, int gen_num, groupid_t gid)
{
    int             err;
    search_state_t *sstate;

    sstate = (search_state_t *) app_cookie;
    err = odisk_set_gid(sstate->ostate, gid);
    assert(err == 0);
    return (0);
}


int
search_clear_gids(void *app_cookie, int gen_num)
{
    int             err;
    search_state_t *sstate;

    /*
     * XXX check gen num 
     */

    sstate = (search_state_t *) app_cookie;
    err = odisk_clear_gids(sstate->ostate);
    assert(err == 0);
    return (0);

}

int
search_set_blob(void *app_cookie, int gen_num, char *name,
                int blob_len, void *blob)
{
    dev_cmd_data_t *cmd;
    int             err;
    search_state_t *sstate;
    void           *new_blob;

    sstate = (search_state_t *) app_cookie;

    cmd = (dev_cmd_data_t *) malloc(sizeof(*cmd));
    if (cmd == NULL) {
        return (1);
    }

    new_blob = malloc(blob_len);
    assert(new_blob != NULL);
    memcpy(new_blob, blob, blob_len);

    cmd->cmd = DEV_BLOB;
    cmd->id = gen_num;

    cmd->extra_data.bdata.fname = strdup(name);
    assert(cmd->extra_data.bdata.fname != NULL);
    cmd->extra_data.bdata.blen = blob_len;
    cmd->extra_data.bdata.blob = new_blob;


    err = ring_enq(sstate->control_ops, (void *) cmd);
    if (err) {
        free(cmd);
        assert(0);
        return (1);
    }
    return (0);
}

extern int      fexec_cpu_slowdown;

int
search_set_offload(void *app_cookie, int gen_num, uint64_t load)
{
    double          ratio;
    uint64_t        my_clock;
    double          my_clock_float;
    double          eff_ratio;

    eff_ratio = (double) (double) (100 - fexec_cpu_slowdown) / 100.0;
    /*
     * XXX clean this up 
     */
    cpu_split = 1;

    my_clock_float = (double) my_clock *eff_ratio;
    r_cpu_freq(&my_clock);
    ratio = ((double) my_clock) / ((double) load + (double) my_clock);

    cpu_split_thresh = (double) (RAND_MAX) * ratio;

#ifdef	XXX
    printf("set_offload: ratio %f thresh %d cpu %d adj %f \n",
           ratio, cpu_split_thresh, my_clock, my_clock_float);
#endif

    return (0);
}
