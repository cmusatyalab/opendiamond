
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
#include <openssl/evp.h>

#include "lib_od.h"
#include "lib_log.h"
#include "lib_dctl.h"
#include "dctl_common.h"
#include "ocache_priv.h"
#include "rtimer.h"
#include "ring.h"
#include "obj_attr.h"
#include "consts.h"
#include "lib_od.h"
#include "obj_attr.h"
#include "lib_filter.h"
#include "lib_odisk.h"
#include "lib_searchlet.h"
#include "filter_exec.h"
#include "lib_ocache.h"

#define	MAX_FNAME	128
#define TEMP_ATTR_BUF_SIZE	1024
#define MAX_ENTRY_NUM	100000

/* dctl variables */
unsigned int if_cache_table = 1;
unsigned int if_cache_oattr = 1;
unsigned int count_thresh = 1;
unsigned int stream_write = 1;

static int		search_active = 0;
static int		search_done = 0;
static int		bg_wait_q = 0;
static int		fg_wait = 0;
static int		nem_wait = 0;
static int		wait_lookup = 0;
static ring_data_t *	cache_ring;
static ring_data_t *	oattr_ring;
static pthread_mutex_t	shared_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t	fg_data_cv = PTHREAD_COND_INITIALIZER; /*queue empty*/
static pthread_cond_t	bg_active_cv = PTHREAD_COND_INITIALIZER; /*active*/
static pthread_cond_t	bg_queue_cv = PTHREAD_COND_INITIALIZER; /*queue full*/
static pthread_cond_t	nem_queue_cv = PTHREAD_COND_INITIALIZER; /*queue non empty*/
static int		oattr_ring_empty = 1;  /* if oattr queue is empty */
static pthread_cond_t	oattr_cv = PTHREAD_COND_INITIALIZER; /*queue non empty*/
static pthread_cond_t	wait_lookup_cv = PTHREAD_COND_INITIALIZER; /*queue non empty*/

#define	CACHE_RING_SIZE	512	
#define	OATTR_RING_SIZE	128	
#define SIG_BUF_SIZE	256
#define MAX_FILTER_ARG_NAME 256
#define CACHE_ENTRY_NUM 500

cache_obj *cache_table[CACHE_ENTRY_NUM];  /* a simple hash table */
static int cache_entry_num = 0;   /* for debug purpose */

static int64_t	ocache_oid = -1;
static int64_t	oattr_oid = -1;

static int
search_paths(const char *filename, char *pathbuf)
{
	const char *envvars[] = {"$DYLD_LIBRARY_PATH",
				 "$LD_LIBRARY_PATH",
				 "/usr/lib:/lib", NULL};
	int envvar_index;
	const char *pathspec;
	const char *element;
	const char *p;
	char *q;
	char *pathbuf_end;
	struct stat stat_buf;

	pathbuf_end = pathbuf + PATH_MAX - 8;

	for(envvar_index = 0; envvars[envvar_index]; envvar_index++){
		if(envvars[envvar_index][0] == '$')
			pathspec = getenv(envvars[envvar_index]+1);
		else
			pathspec = envvars[envvar_index];
		if(pathspec != NULL){
			element = pathspec;
			while(*element){
				p = element;
				q = pathbuf;
				while(*p && *p != ':' && q < pathbuf_end) 
					*q++ = *p++;
				if(q == pathbuf){  /* empty element */
					if(*p){
						element = p+1;
						continue;
					}
					break;
				}
				if (*p)
					element = p+1;
				else
					element = p;
				if(*(q-1) != '/' && q < pathbuf_end)
					*q++ = '/';
				p = filename;
				while(*p && q < pathbuf_end) 
					*q++ = *p++;
				*q++ = 0;
				if(q >= pathbuf_end)
					break;
				if(stat(pathbuf, &stat_buf) == 0)
					return(0);
			}
		}
	}
	return(-1);
}

int
digest_cal(char *lib_name, char *filt_name, int numarg, char **filt_args, int blob_len, void *blob, unsigned char ** signature)
{
	EVP_MD_CTX mdctx;
	const EVP_MD *md;
	unsigned char *md_value;
	int md_len=0, i, len;
	int lib_fd;
	char buf[SIG_BUF_SIZE];
	char pathbuf[PATH_MAX];

	OpenSSL_add_all_digests();
	md = EVP_get_digestbyname("md5");
	if(!md) {
		printf("Unknown message digest md5\n");
		exit(1);
	}

	md_value = *signature;

	if( lib_name == NULL )
		printf("null lib_name\n");
	if( lib_name[0] == '/' ) {
		lib_fd = open(lib_name, O_RDONLY);
	} else {
		search_paths(lib_name, pathbuf);
		lib_fd = open(pathbuf, O_RDONLY);
	}
	if(lib_fd < 0) {
		printf("fail to open lib file %s, errno %d\n", lib_name,errno);
		assert(0);
		return(EINVAL);
	}

	EVP_MD_CTX_init(&mdctx);
	EVP_DigestInit_ex(&mdctx, md, NULL);

	do {
		len = read(lib_fd, buf, SIG_BUF_SIZE);
		EVP_DigestUpdate(&mdctx, buf, len);
	} while( len > 0 );

	close(lib_fd);

	len = strlen(filt_name);
        if (len >= MAX_FILTER_FUNC_NAME) {
                /* XXX error */
                return(EINVAL);
        }

	EVP_DigestUpdate(&mdctx, filt_name, len);
	for(i=0; i<numarg; i++) {
		len = strlen(filt_args[i]);
		if (len >= MAX_FILTER_ARG_NAME) {
			return(EINVAL);
		}
		EVP_DigestUpdate(&mdctx, filt_args[i], len);
	}
	if( blob_len > 0 ) 
		EVP_DigestUpdate(&mdctx, blob, blob_len);

	EVP_DigestFinal_ex(&mdctx, md_value, &md_len);
	EVP_MD_CTX_cleanup(&mdctx);

/*
	printf("Digest is: ");
	for(i = 0; i < md_len; i++) 
		printf("%x", md_value[i]);
	printf("\n");
*/

	if( md_len == 16 )
		return(0);
	else
		return(EINVAL);
}

int
sig_cal(void *buf, off_t buflen, unsigned char **signature)
{
	EVP_MD_CTX mdctx;
	const EVP_MD *md;
	unsigned char *md_value;
	int md_len=0;

	OpenSSL_add_all_digests();
	md = EVP_get_digestbyname("md5");
	if(!md) {
		printf("Unknown message digest md5\n");
		exit(1);
	}

	md_value = *signature;

	EVP_MD_CTX_init(&mdctx);
	EVP_DigestInit_ex(&mdctx, md, NULL);

	EVP_DigestUpdate(&mdctx, buf, buflen);

	EVP_DigestFinal_ex(&mdctx, md_value, &md_len);
	EVP_MD_CTX_cleanup(&mdctx);

	if( md_len == 16 )
		return(0);
	else
		return(EINVAL);
}

static int
compare_attr_set(cache_attr_set *attr1, cache_attr_set *attr2)
{
	int i, j;
	cache_attr_entry *temp_i, *temp_j;

	for(i=0; i < attr1->entry_num; i++) {
		temp_i = attr1->entry_data[i];
		if( temp_i == NULL ) {
			printf("null temp_i, something wrong\n");
			continue;
		}
		for(j=0; j < attr2->entry_num; j++) {
			temp_j = attr2->entry_data[j];
			if( temp_j == NULL ) {
				printf("null temp_j, something wrong\n");
				continue;
			}
			if( (temp_i->name_len == temp_j->name_len) &&
			!strncmp(temp_i->attr_name, temp_j->attr_name, temp_i->name_len) 
			&& strncmp(temp_i->attr_sig, temp_j->attr_sig, 16) ) {
				//printf("changed attr %s\n", temp_i->attr_name);
				return 1;
			}
		}
	}
	return 0;
}

static void
dump_attr_set(cache_attr_set *attr)
{
	int i;
	cache_attr_entry *temp_i;
	
	for(i=0; i < attr->entry_num; i++) {
		temp_i = attr->entry_data[i];
		printf("attr %s\n", temp_i->attr_name);
	}
}

int
combine_attr_set(cache_attr_set *attr1, cache_attr_set *attr2)
{
	int i, j;
	int found;
	cache_attr_entry *temp_i, *temp_j;
	cache_attr_entry **tmp;

	for(i=0; i < attr2->entry_num; i++) {
		temp_i = attr2->entry_data[i];
		if( temp_i == NULL ) {
			printf("null temp_i, something wrong\n");
			continue;
		}
		found = 0;
		for(j=0; j < attr1->entry_num; j++) {
			temp_j = attr1->entry_data[j];
			if( temp_j == NULL ) {
				printf("null temp_j, something wrong\n");
				continue;
			}
			if( (temp_i->name_len == temp_j->name_len) &&
			!strncmp(temp_i->attr_name, temp_j->attr_name, temp_i->name_len) ) {
				attr1->entry_data[j] = temp_i;
				found = 1;
				break;
			}
		}
		/* no found, add to the tail */
		if( !found ) {
			attr1->entry_data[attr1->entry_num] = temp_i;
			attr1->entry_num++;
			if( (attr1->entry_num % ATTR_ENTRY_NUM) == 0 ) {
				/* enlarge the memory */
				tmp = malloc( (attr1->entry_num + ATTR_ENTRY_NUM) * sizeof(char *) );
				memcpy(tmp, attr1->entry_data, attr1->entry_num * sizeof(char *) );
				free(attr1->entry_data);
				attr1->entry_data = tmp;
			}
		}
	}
	return 0;
}

static int
ocache_entry_free(cache_obj *cobj)
{
	int i;

        for(i=0; i < cobj->iattr.entry_num; i++) {
		free(cobj->iattr.entry_data[i]->attr_name);
		free(cobj->iattr.entry_data[i]);
        }
	free(cobj->iattr.entry_data);
        for(i=0; i < cobj->oattr.entry_num; i++) {
		free(cobj->oattr.entry_data[i]->attr_name);
		free(cobj->oattr.entry_data[i]);
        }
	free(cobj->oattr.entry_data);
	if( cobj->file_name != NULL )
		free(cobj->file_name);
	free(cobj);
	return (0);
}

/* XXX: need some better way for lookup later. but for now, just something simple */
int 
ocache_lookup(uint64_t local_id, char *fsig, cache_attr_set *change_attr, int *err, cache_attr_set **oattr_set, char **fpath)
{
	cache_obj *cobj;
	int found=0;
	unsigned int index;

	pthread_mutex_lock(&shared_mutex);

	index = local_id % CACHE_ENTRY_NUM;
	cobj = cache_table[index];
	*fpath = NULL;
	/* cache hit if there is a (oid, filter sig, input attr sig) match */
	while( cobj != NULL ) {
		if( (cobj->oid == local_id) && !strncmp(cobj->filter_sig, fsig, 16)) {
			/* compare change_attr set with input attr set */
			if( !compare_attr_set(&cobj->iattr, change_attr) ) {
				found = 1;
				*err = cobj->result;
				*fpath = cobj->file_name;
				/* pass back the output attr set for next evaluation */
				*oattr_set = &cobj->oattr;
				break;
			} else {
			/*
				printf("INPUT ATTR SET : \n");
				dump_attr_set(&cobj->iattr);
				printf("CHANGE ATTR SET : \n");
				dump_attr_set(change_attr);
			*/
			}
		}
		cobj = cobj->next;
	}

	pthread_mutex_unlock(&shared_mutex);

	return found;
}

int 
ocache_lookup2(uint64_t local_id, char *fsig, cache_attr_set *change_attr, int *conf, cache_attr_set **oattr_set, char **fpath, int flag)
{
	cache_obj *cobj;
	unsigned int index;
	int err = ENOENT;
	int oattr_count = 1;
	char *fname;
	unsigned int random_num;
	uint64_t tmp;

	pthread_mutex_lock(&shared_mutex);
	index = local_id % CACHE_ENTRY_NUM;
	cobj = cache_table[index];
	*fpath = NULL;
	/* cache hit if there is a (oid, filter sig, input attr sig) match */
	while( cobj != NULL ) {
		if( (cobj->oid == local_id) && !strncmp(cobj->filter_sig, fsig, 16)) {
			/* compare change_attr set with input attr set */
			if( !compare_attr_set(&cobj->iattr, change_attr) ) {
				/* XXX add max number check later */
				cobj->eval_count++; 
				oattr_count = cobj->eval_count;
				*oattr_set = &cobj->oattr;
				*conf = cobj->result;
				err = 0;
				//printf("find cached entry count %d\n", oattr_count);
				break;
			}
		}
		cobj = cobj->next;
	}

if (if_cache_oattr) {
	if( (oattr_count >= count_thresh) && flag ) {
		/* generate a unique file name with the oid&fsig&random_num */
		fname = (char *)malloc( 16 + 16 + 6 + 3);
		memcpy( &tmp, fsig, sizeof(tmp) );
		random_num = (unsigned int) (rand()/1024);
		/* XXX: check if the file exists */
		sprintf(fname, "%016llX.%016llX.%06X\0", local_id, tmp, random_num);
		if( cobj != NULL ) {
			if( cobj->file_name != NULL )
				free(cobj->file_name);
			cobj->file_name = fname;
		}
		*fpath = fname;
	}
}

	pthread_mutex_unlock(&shared_mutex);
	return (err);
}

int 
ocache_wait_lookup(obj_data_t *lobj, char *fsig, cache_attr_set *change_attr, cache_attr_set **oattr_set)
{
   cache_obj *cobj;
   unsigned int index;

   *oattr_set = NULL;
   if( if_cache_table ) {
	/* XXX we should set a timer here */
	while( *oattr_set == NULL ) {
		pthread_mutex_lock(&shared_mutex);
		index = lobj->local_id % CACHE_ENTRY_NUM;
		cobj = cache_table[index];
		/* cache hit if there is a (oid, filter sig, input attr sig) match */
		while( cobj != NULL ) {
			if( (cobj->oid == lobj->local_id) && !strncmp(cobj->filter_sig, fsig, 16)) {
				/* compare change_attr set with input attr set */
				if( !compare_attr_set(&cobj->iattr, change_attr) ) {
					*oattr_set = &cobj->oattr;
					break;
				}
			}
			cobj = cobj->next;
		}
		if( *oattr_set == NULL ) {
			wait_lookup = 1;
			pthread_cond_wait(&wait_lookup_cv, &shared_mutex);
		}
		pthread_mutex_unlock(&shared_mutex);
	}
	return (0);
   }
   return (ENOENT);
}

/* XXX: now just assume there is only one cache file,
 * but later we want to do this on finer granularity. */
static int
ocache_read_file(char *disk_path)
{
        char fpath[PATH_MAX];
        cache_obj *cobj;
	int i;
	off_t size, rsize;
	int fd;
	struct stat     stats;
	cache_obj *p, *q;
	unsigned int index;
	int err;
	unsigned int fname_len;

        sprintf(fpath, "%s/%s", disk_path, CACHE_EXT);
	for( i=0; i<CACHE_ENTRY_NUM; i++ ) {
		cache_table[i] = NULL;
	}

	fd = open(fpath, O_RDONLY, 00777);
        if( fd < 0 ) {
		perror("failed to open cache file\n");
                return(0);
        }
	err = fstat( fd, &stats );
        if( err != 0 ) {
		perror("failed to open cache file\n");
                return(0);
        }
	size = stats.st_size;
	printf("ocache_read_file size %ld\n", size);
	rsize = 0;

        pthread_mutex_lock(&shared_mutex);
	while( (rsize < size) && (cache_entry_num < MAX_ENTRY_NUM) ) {
        	cobj = (cache_obj *) malloc(sizeof(*cobj));
        	read(fd, &cobj->oid, sizeof(uint64_t));
        	read(fd, cobj->filter_sig, 16);
        	read(fd, &fname_len, sizeof(unsigned int));
		if( fname_len == 0 ) {
			cobj->file_name = NULL;
		} else {
			cobj->file_name = malloc(fname_len + 1);
        		read(fd, cobj->file_name, fname_len);
			cobj->file_name[fname_len] = '\0';
		}
        	read(fd, &cobj->result, sizeof(int));
        	read(fd, &cobj->eval_count, sizeof(int));
		rsize += (sizeof(uint64_t)+16+sizeof(unsigned int)+fname_len+sizeof(int)+sizeof(int));

        	read(fd, &cobj->iattr.entry_num, sizeof(unsigned int));
		rsize += sizeof(unsigned int);
        	cobj->iattr.entry_data = malloc( cobj->iattr.entry_num*sizeof(char *) );
        	for(i=0; i < cobj->iattr.entry_num; i++) {
			cobj->iattr.entry_data[i] = malloc( sizeof(cache_attr_entry) );
                	read(fd, &cobj->iattr.entry_data[i]->name_len, sizeof(unsigned int));
			cobj->iattr.entry_data[i]->attr_name = malloc(cobj->iattr.entry_data[i]->name_len + 1);
                	read(fd, cobj->iattr.entry_data[i]->attr_name, cobj->iattr.entry_data[i]->name_len);
			cobj->iattr.entry_data[i]->attr_name[cobj->iattr.entry_data[i]->name_len] = '\0';
                	read(fd, cobj->iattr.entry_data[i]->attr_sig, 16);
			rsize += (sizeof(unsigned int) + cobj->iattr.entry_data[i]->name_len + 16);
        	}

        	read(fd, &cobj->oattr.entry_num, sizeof(unsigned int));
		rsize += sizeof(unsigned int);
        	cobj->oattr.entry_data = malloc( cobj->oattr.entry_num*sizeof(char *) );
        	for(i=0; i < cobj->oattr.entry_num; i++) {
			cobj->oattr.entry_data[i] = malloc( sizeof(cache_attr_entry) );
                	read(fd, &cobj->oattr.entry_data[i]->name_len, sizeof(unsigned int));
			cobj->oattr.entry_data[i]->attr_name = malloc(cobj->oattr.entry_data[i]->name_len + 1);
                	read(fd, cobj->oattr.entry_data[i]->attr_name, cobj->oattr.entry_data[i]->name_len);
			cobj->oattr.entry_data[i]->attr_name[cobj->oattr.entry_data[i]->name_len] = '\0';
                	read(fd, cobj->oattr.entry_data[i]->attr_sig, 16);
			rsize += (sizeof(unsigned int) + cobj->oattr.entry_data[i]->name_len + 16);
        	}
		cobj->next = NULL;
		/* insert it into the cache_table array */
		index = cobj->oid % CACHE_ENTRY_NUM;
		if( cache_table[index] == NULL ) {
			cache_table[index] = cobj;
		} else {
			p = cache_table[index];
			while( p!= NULL ) {
				q = p;
				p = p->next;
			}
			q->next = cobj;
		}
		/* for debug purpose */
		cache_entry_num++;

	}
        pthread_mutex_unlock(&shared_mutex);

	printf("cache_entry_num %d\n", cache_entry_num);
	close(fd);
	return (0);
}

static int
ocache_write_file(char *disk_path)
{
        char fpath[PATH_MAX];
        cache_obj *cobj;
        cache_obj *tmp;
	int i, j;
	int fd;
	unsigned int fname_len;

	printf("ocache_write_file start\n");
        sprintf(fpath, "%s/%s", disk_path, CACHE_EXT);

	fd = open(fpath, O_CREAT|O_RDWR, 00777);
        if( fd < 0 ) {
		perror("failed to open cache file\n");
                return(0);
        }

        pthread_mutex_lock(&shared_mutex);
	for( j=0; j<CACHE_ENTRY_NUM; j++ ) {
		cobj = cache_table[j];
		while( cobj != NULL ) {
        		write(fd, &cobj->oid, sizeof(uint64_t));
        		write(fd, cobj->filter_sig, 16);
			if( cobj->file_name != NULL ) {
				fname_len = strlen(cobj->file_name);
        			write(fd, &fname_len, sizeof(unsigned int));
        			write(fd, cobj->file_name, fname_len);
			} else {
				fname_len = 0;
        			write(fd, &fname_len, sizeof(unsigned int));
			}
        		write(fd, &cobj->result, sizeof(int));
        		write(fd, &cobj->eval_count, sizeof(int));

        		write(fd, &cobj->iattr.entry_num, sizeof(unsigned int));
        		for(i=0; i < cobj->iattr.entry_num; i++) {
                		write(fd, &cobj->iattr.entry_data[i]->name_len, sizeof(unsigned int));
                		write(fd, cobj->iattr.entry_data[i]->attr_name, cobj->iattr.entry_data[i]->name_len);
                		write(fd, cobj->iattr.entry_data[i]->attr_sig, 16);
        		}

        		write(fd, &cobj->oattr.entry_num, sizeof(unsigned int));
        		for(i=0; i < cobj->oattr.entry_num; i++) {
                		write(fd, &cobj->oattr.entry_data[i]->name_len, sizeof(unsigned int));
                		write(fd, cobj->oattr.entry_data[i]->attr_name, cobj->oattr.entry_data[i]->name_len);
                		write(fd, cobj->oattr.entry_data[i]->attr_sig, 16);
        		}
			
			tmp = cobj;
			cobj = cobj->next;
			/* free */
			ocache_entry_free(tmp);
		}
	}
	cache_entry_num = 0;
	for( i=0; i<CACHE_ENTRY_NUM; i++ ) {
		cache_table[i] = NULL;
	}
	pthread_mutex_unlock(&shared_mutex);
	close(fd);
	printf("ocache_write_file done\n");
	return (0);
}

static int
ocache_lookup_next(cache_ring_entry **cobj, ocache_state_t *ocache)
{
	pthread_mutex_lock(&shared_mutex);
	while (1) {
		if (!ring_empty(cache_ring)) {
			*cobj =  ring_deq(cache_ring);	
			if (bg_wait_q) {
				bg_wait_q = 0;
				pthread_cond_signal(&bg_queue_cv);
			}
			pthread_mutex_unlock(&shared_mutex);
			return(0);
		} else {
			if( nem_wait ) {
				nem_wait = 0;
				pthread_cond_signal(&nem_queue_cv);
			}
			fg_wait = 1;
			pthread_cond_wait(&fg_data_cv, &shared_mutex);
		}
	} 
}

static int
oattr_lookup_next(oattr_ring_entry **cobj, ocache_state_t *ocache)
{
	pthread_mutex_lock(&shared_mutex);
	while (1) {
		if (!ring_empty(oattr_ring)) {
			*cobj =  ring_deq(oattr_ring);	
			pthread_mutex_unlock(&shared_mutex);
			return(1);
		} else {
			oattr_ring_empty = 1;
			while (oattr_ring_empty == 1) {
				pthread_cond_wait(&oattr_cv, &shared_mutex);
			}
		}
	}
}

static int
ocache_ring_insert(cache_ring_entry *cobj)
{
	pthread_mutex_lock(&shared_mutex);
	if( !ring_full(cache_ring) ) {
		ring_enq(cache_ring, cobj);
	} else { 
		bg_wait_q = 1;
		pthread_cond_wait(&bg_queue_cv, &shared_mutex);
		ring_enq(cache_ring, cobj);
	}
	if( fg_wait ) {
		fg_wait = 0;
		pthread_cond_signal(&fg_data_cv);
	}
	pthread_mutex_unlock(&shared_mutex);
	return(0);
}


static int
oattr_ring_insert(oattr_ring_entry *cobj)
{
	pthread_mutex_lock(&shared_mutex);  //XXX: do we need another mutex?
	/* we do not wait if ring full. just drop it */
	if( !ring_full(oattr_ring) ) {
		ring_enq(oattr_ring, cobj);
	} else {
		if(cobj->type == INSERT_OATTR) {
			free(cobj->u.oattr.name);
			free(cobj->u.oattr.data);
			free(cobj);
		} else if(cobj->type == INSERT_START) {
			free(cobj->u.file_name);
			free(cobj);
		} else {
			free(cobj);
		}
	}
	if( oattr_ring_empty == 1 ) {
		oattr_ring_empty = 0;
		pthread_cond_signal(&oattr_cv);
	}
	pthread_mutex_unlock(&shared_mutex);
	return(0);
}

int
ocache_add_start(char *fhandle, uint64_t obj_id, unsigned char *fsig, int lookup, char *fpath)
{
	cache_ring_entry	*new_entry;
	oattr_ring_entry	*oattr_entry;

	//printf("ocache_add_start: lookup %d, fpath %s\n", lookup, fpath);
if (if_cache_table) {
   if( lookup == ENOENT ) {
		ocache_oid = obj_id;
		new_entry = (cache_ring_entry *) malloc( sizeof( *new_entry) );
		if( new_entry == NULL )
			return (ENOMEM);
		new_entry->type = INSERT_START;
		new_entry->oid = obj_id;
		memcpy(new_entry->u.start.filter_sig, fsig, 16);
		new_entry->u.start.file_name = fpath;

		if( fpath != NULL ) {
		} else {
			new_entry->u.start.file_name = NULL;
		}
		ocache_ring_insert(new_entry);
	}
}

if (if_cache_oattr) {
	if( fpath != NULL ) {
		oattr_oid = obj_id;
		oattr_entry = (oattr_ring_entry *) malloc( sizeof(*oattr_entry) );
		if( oattr_entry == NULL )
			return (ENOMEM);
		oattr_entry->type = INSERT_START;
		oattr_entry->oid = obj_id;
		oattr_entry->u.file_name = (char *) malloc(16 + 16 + 6 + 3);
		memcpy(oattr_entry->u.file_name, fpath, 16 + 16 + 6 + 3);
		oattr_ring_insert(oattr_entry);
	}
}

	return(0);
}

static void
ocache_add_iattr(char *fhandle, uint64_t obj_id, const char *name, off_t len, char *data)
{
	cache_ring_entry	*new_entry;
	unsigned char *sig;

	//printf("ocache_add_iattr %s\n", name);
if (if_cache_table) {
	if( ocache_oid == obj_id ) {
		new_entry = (cache_ring_entry *) malloc( sizeof( *new_entry) );
		if( new_entry == NULL ) {
			printf("ENOMEM\n");
			return;
		}
		new_entry->type = INSERT_IATTR;
		new_entry->oid = obj_id;
		new_entry->u.iattr.name_len = strlen(name);
		new_entry->u.iattr.attr_name = malloc( strlen(name)+1 );
		if( new_entry->u.iattr.attr_name == NULL ) {
			printf("ENOMEM\n");
			return;
		}
		memcpy( new_entry->u.iattr.attr_name, name, strlen(name) );
		new_entry->u.iattr.attr_name[strlen(name)] = '\0';
		sig = (unsigned char *) malloc(16);
		if( sig == NULL ) {
			printf("ENOMEM\n");
			return;
		}
		sig_cal( data, len, &sig );
		memcpy(new_entry->u.iattr.attr_sig, sig, 16);
		free( sig );
		ocache_ring_insert(new_entry);
	}
}

	return;
}

static void
ocache_add_oattr(char *fhandle, uint64_t obj_id, const char *name, off_t len, char *data)
{
	cache_ring_entry	*new_entry;
	oattr_ring_entry	*oattr_entry;
	unsigned char *sig;

	//printf("ocache_add_oattr %s\n", name);
if (if_cache_table) {
	if( ocache_oid == obj_id ) {
		new_entry = (cache_ring_entry *) malloc( sizeof( *new_entry) );
		if( new_entry == NULL ) {
			printf("ENOMEM\n");
			return;
		}
		new_entry->type = INSERT_OATTR;
		new_entry->oid = obj_id;
		new_entry->u.iattr.name_len = strlen(name);
		new_entry->u.iattr.attr_name = malloc( strlen(name)+1 );
		if( new_entry->u.iattr.attr_name == NULL ) {
			printf("ENOMEM\n");
			return;
		}
		memcpy( new_entry->u.iattr.attr_name, name, strlen(name) );
		new_entry->u.iattr.attr_name[strlen(name)] = '\0';
		sig = (unsigned char *) malloc(16);
		if( sig == NULL ) {
			printf("ENOMEM\n");
			return;
		}
		sig_cal( data, len, &sig );
		memcpy(new_entry->u.iattr.attr_sig, sig, 16);
		free( sig );
		ocache_ring_insert(new_entry);
	}
}

if (if_cache_oattr) {
	if( oattr_oid == obj_id ) {
		oattr_entry = (oattr_ring_entry *) malloc( sizeof(*oattr_entry) );
		if( oattr_entry == NULL ) {
			printf("ENOMEM\n");
			return;
		}
		oattr_entry->type = INSERT_OATTR;
		oattr_entry->oid = obj_id;
		oattr_entry->u.oattr.name_len = strlen(name);
		oattr_entry->u.oattr.name = (char *) malloc( strlen(name) );
		memcpy(oattr_entry->u.oattr.name, name, strlen(name) );
		oattr_entry->u.oattr.data_len = len;
		oattr_entry->u.oattr.data = (char *) malloc( len );
		memcpy( oattr_entry->u.oattr.data, data, len );
		oattr_ring_insert(oattr_entry);
	}
}

	return;
}

int
ocache_add_end(char *fhandle, uint64_t obj_id, int conf)
{
	cache_ring_entry	*new_entry;
	oattr_ring_entry	*oattr_entry;

if (if_cache_table) {
	if( ocache_oid == obj_id ) {
		new_entry = (cache_ring_entry *) malloc( sizeof( *new_entry) );
		if( new_entry == NULL )
			return (ENOMEM);
		new_entry->type = INSERT_END;
		new_entry->oid = obj_id;
		new_entry->u.result = conf;
		ocache_ring_insert(new_entry);
		ocache_oid = -1;
	}
}

if (if_cache_oattr) {
	if( oattr_oid == obj_id ) {
		oattr_entry = (oattr_ring_entry *) malloc( sizeof(*oattr_entry) );
		if( oattr_entry == NULL )
			return (ENOMEM);
		oattr_entry->type = INSERT_END;
		oattr_entry->oid = obj_id;
		oattr_ring_insert(oattr_entry);
		oattr_oid = -1;
	}
}

	return(0);
}

static void *
ocache_main(void *arg)
{
	ocache_state_t  *	cstate = (ocache_state_t *)arg;
	int			err;
	cache_ring_entry * tobj;
	cache_obj * cobj;
	cache_obj 	*p, *q;
	cache_attr_entry **tmp;
	unsigned int index;
	int correct;
/*
	pthread_t tid;
	int prio;
	int policy;
	struct sched_param param;
*/
	dctl_thread_register(cstate->dctl_cookie);
	log_thread_register(cstate->log_cookie);

/*
	tid = pthread_self();
	param.sched_priority = 5;
	err = pthread_setschedparam(tid, SCHED_RR, &param);
	printf("ocache set priority err %d\n", err);
*/

	while (1) {
		/* If there is no search don't do anything */
		pthread_mutex_lock(&shared_mutex);
		while (search_active == 0) {
			err = pthread_cond_wait(&bg_active_cv, &shared_mutex);
		}
		pthread_mutex_unlock(&shared_mutex);
		
		/*get the next lookup object*/
		err = ocache_lookup_next(&tobj, cstate);
		/* for one thread case, we could do it in this simple way.
		 * XXX: do we need to change this later? */
		if(tobj->type == INSERT_START) {
			correct = 0;
			cobj = (cache_obj *) malloc(sizeof(*cobj));
			cobj->oid = tobj->oid;
			cobj->eval_count = 1;
			memcpy( cobj->filter_sig, tobj->u.start.filter_sig, 16 );
			cobj->file_name = tobj->u.start.file_name;
			free(tobj);

			cobj->iattr.entry_num = 0;
			cobj->iattr.entry_data = malloc(ATTR_ENTRY_NUM * sizeof(char *) );
			if( cobj->iattr.entry_data == NULL ) 
				exit(1);

			cobj->oattr.entry_num = 0;
			cobj->oattr.entry_data = malloc(ATTR_ENTRY_NUM * sizeof(char *) );
			if( cobj->oattr.entry_data == NULL ) 
				exit(1);

			while(1) {
				err = ocache_lookup_next(&tobj, cstate);
				if(tobj->type == INSERT_IATTR) {
					if( cobj->oid != tobj->oid ) {
						printf("iattr: cobj->oid %lld, tobj->oid %lld\n", cobj->oid, tobj->oid);
						free(tobj);
						break;
					}
					cobj->iattr.entry_data[cobj->iattr.entry_num] = (cache_attr_entry *) malloc( sizeof(cache_attr_entry) );
					cobj->iattr.entry_data[cobj->iattr.entry_num]->name_len = tobj->u.iattr.name_len;
					cobj->iattr.entry_data[cobj->iattr.entry_num]->attr_name = tobj->u.iattr.attr_name;
					memcpy(cobj->iattr.entry_data[cobj->iattr.entry_num]->attr_sig, &tobj->u.iattr.attr_sig, 16 );
					cobj->iattr.entry_num++;
					if( (cobj->iattr.entry_num % ATTR_ENTRY_NUM) == 0 ) {
						/* enlarge the memory */
						tmp = malloc( (cobj->iattr.entry_num + ATTR_ENTRY_NUM) * sizeof(char *) );
						memcpy(tmp, cobj->iattr.entry_data, cobj->iattr.entry_num * sizeof(char *) );
						free(cobj->iattr.entry_data);
						cobj->iattr.entry_data = tmp;
					}
					free(tobj);
				}
				if(tobj->type == INSERT_OATTR) {
					if( cobj->oid != tobj->oid ) {
						printf("oattr: cobj->oid %lld, tobj->oid %lld\n", cobj->oid, tobj->oid);
						free(tobj);
						break;
					}
					cobj->oattr.entry_data[cobj->oattr.entry_num] = (cache_attr_entry *) malloc( sizeof(cache_attr_entry) );
					cobj->oattr.entry_data[cobj->oattr.entry_num]->name_len = tobj->u.oattr.name_len;
					cobj->oattr.entry_data[cobj->oattr.entry_num]->attr_name = tobj->u.oattr.attr_name;
					memcpy(cobj->oattr.entry_data[cobj->oattr.entry_num]->attr_sig, &tobj->u.oattr.attr_sig, 16 );
					cobj->oattr.entry_num++;
					if( (cobj->oattr.entry_num % ATTR_ENTRY_NUM) == 0 ) {
						/* enlarge the memory */
						tmp = malloc( (cobj->oattr.entry_num + ATTR_ENTRY_NUM) * sizeof(char *) );
						memcpy(tmp, cobj->oattr.entry_data, cobj->oattr.entry_num * sizeof(char *) );
						free(cobj->oattr.entry_data);
						cobj->oattr.entry_data = tmp;
					}
					free(tobj);
				}
				if(tobj->type == INSERT_END) {
					if( cobj->oid != tobj->oid ) {
						printf("end: cobj->oid %lld, tobj->oid %lld\n", cobj->oid, tobj->oid);
						free(tobj);
						break;
					}
					cobj->result = tobj->u.result;
					correct = 1;
					free(tobj);
					break;
				}
			}
			/* insert into cache table */ 
			/* now we just drop the latest inserted entry.
			 * we should do something smarter later */
			if( cache_entry_num < MAX_ENTRY_NUM ) {
				cobj->next = NULL;
				index = cobj->oid % CACHE_ENTRY_NUM;

				//printf("insert cache entry: oid %016llX\n", cobj->oid);

				pthread_mutex_lock(&shared_mutex);
				if( cache_table[index] == NULL ) {
					cache_table[index] = cobj;
				} else {
					p = cache_table[index];
					while( p!= NULL ) {
						q = p;
						p = p->next;
					}
					q->next = cobj;
				}
				/* for debug purpose 
				cache_entry_num++;
				if( (cache_entry_num % 50) == 0 )
					printf("cache_entry_num %d\n", cache_entry_num);
				*/
				if( wait_lookup ) {
					wait_lookup = 0;
					pthread_cond_signal(&wait_lookup_cv);
				}
				pthread_mutex_unlock(&shared_mutex);
			}
		} else {
			free(tobj);
		}
	}
}

static void *
oattr_main(void *arg)
{
	ocache_state_t  *	cstate = (ocache_state_t *)arg;
	oattr_ring_entry * tobj;
	uint64_t                oid;
	char *fname;
	int fd;
	FILE *file;
	char attrbuf[PATH_MAX];
	int err;
	int correct;
/*
	pthread_t tid;
	int prio;
	int policy;
	struct sched_param param;

	tid = pthread_self();
	param.sched_priority = 1;
	err = pthread_setschedparam(tid, SCHED_RR, &param);
	printf("oattr set priority err %d\n", err);
*/
	while (1) {
		/* If there is no search don't do anything */
	/*
		pthread_mutex_lock(&shared_mutex);
		while (oattr_ring_empty == 1) {
			err = pthread_cond_wait(&oattr_cv, &shared_mutex);
		}
		pthread_mutex_unlock(&shared_mutex);
	*/	
		//if( search_active )
			//usleep(100000);
		err = oattr_lookup_next(&tobj, cstate);
		if( err != 1 )
			continue;

		if(tobj->type == INSERT_START) {
			correct = 0;
			oid = tobj->oid;
			fname = tobj->u.file_name;
			//printf("oattr_main fname %s\n", fname);
			sprintf(attrbuf, "%s/%s/%s", cstate->ocache_path, CACHE_DIR, 
				fname);
			free(fname);
			free(tobj);

if( stream_write == 1 ) {
			err = access(attrbuf, F_OK);
			if( err == 0 ) {
				printf("file already exists\n");
				continue;
			}
			file = fopen(attrbuf, "w+");
			if( file == NULL ) {
				printf("failed in open oattr file %s for oid %016llX\n", attrbuf, oid);
				perror("error");
				continue;
			}
} else {
			fd = open(attrbuf, O_WRONLY|O_CREAT|O_EXCL, 00777);
			if( fd < 0 ) {
				if( errno == EEXIST ) {
					printf("file already exists\n");
					continue;
				} else {
					printf("failed in open oattr file %s for oid %016llX\n", attrbuf, oid);
					perror("error");
					continue;
				}
			}
}

			while(1) {
				err = oattr_lookup_next(&tobj, cstate);
				if( err != 1 ) {
					printf("something wrong from oattr_lookup_next\n");
					break;
				}
				if( oid != tobj->oid ) {
					printf("oattr_main: oid %lld, tobj->oid %lld\n", oid, tobj->oid);
					free(tobj);
					break;
				}
				if(tobj->type == INSERT_END) {
					free(tobj);
					correct = 1;
					break;
				}
				if(tobj->type != INSERT_OATTR) {
					printf("something wrong in oattr\n");
					free(tobj);
					break;
				}
if( stream_write == 1 ) {
				err = fwrite(&tobj->u.oattr.name_len, sizeof(unsigned int), 1, file);
				if( err != 1 )
					break;
				err = fwrite(tobj->u.oattr.name, tobj->u.oattr.name_len, 1, file);
				if( err != 1 )
					break;
				err = fwrite(&tobj->u.oattr.data_len, sizeof(off_t), 1, file);
				if( err != 1 )
					break;
				err = fwrite(tobj->u.oattr.data, tobj->u.oattr.data_len, 1, file);
				if( err != 1 )
					break;
} else {
				write(fd, &tobj->u.oattr.name_len, sizeof(unsigned int));
				write(fd, tobj->u.oattr.name, tobj->u.oattr.name_len);
				write(fd, &tobj->u.oattr.data_len, sizeof(off_t));
				write(fd, tobj->u.oattr.data, tobj->u.oattr.data_len);
}

				free(tobj->u.oattr.name);
				free(tobj->u.oattr.data);
				free(tobj);
			}

if( stream_write == 1 ) {
			fclose(file);
} else {
			close(fd);
}

			if( correct == 0 )
				unlink(attrbuf);
		}
	}
}

int
ocache_init(char *dir_path, void *dctl_cookie, void *log_cookie)
{ 
	ocache_state_t  *	new_state;
	int			err;
	char buf[PATH_MAX];

	if (strlen(dir_path) > (MAX_DIR_PATH-1)) {
		/* XXX log */
		return(EINVAL);
	}
	sprintf( buf, "%s/%s", dir_path, CACHE_DIR );
	err = mkdir( buf, 0x777);
	if( err && errno != EEXIST ) {
		printf("fail to creat cache dir, err %d\n", err);
		return(EPERM);
	}

	/* dctl control */
	dctl_register_leaf(DEV_CACHE_PATH, "cache_table", DCTL_DT_UINT32,
			dctl_read_uint32, dctl_write_uint32,
			&if_cache_table);
	
	dctl_register_leaf(DEV_CACHE_PATH, "cache_oattr", DCTL_DT_UINT32,
			dctl_read_uint32, dctl_write_uint32,
			&if_cache_oattr);
	
	dctl_register_leaf(DEV_CACHE_PATH, "cache_thresh_hold", DCTL_DT_UINT32,
			dctl_read_uint32, dctl_write_uint32,
			&count_thresh);
	
	dctl_register_leaf(DEV_CACHE_PATH, "stream_write", DCTL_DT_UINT32,
			dctl_read_uint32, dctl_write_uint32,
			&stream_write);
	
	ring_init(&cache_ring, CACHE_RING_SIZE);
	/* creat output attr ring */
	ring_init(&oattr_ring, OATTR_RING_SIZE);

	new_state = (ocache_state_t *)malloc(sizeof(*new_state));
	if (new_state == NULL) {
		/* XXX err log */
		return(ENOMEM);
	}

	memset(new_state, 0, sizeof(*new_state));

	new_state->dctl_cookie = dctl_cookie;
	new_state->log_cookie = log_cookie;

	lf_set_read_cb(ocache_add_iattr);
	lf_set_write_cb(ocache_add_oattr);

	/* the length has already been tested above */
	strcpy(new_state->ocache_path, dir_path);	
	
	/* read in cache_table */
	ocache_read_file(dir_path);

	/* create thread to process inserted entries for cache table */
	err = pthread_create(&new_state->c_thread_id, PATTR_DEFAULT, 
		ocache_main, (void *) new_state);

	/* create thread to process inserted output attrs */
	err = pthread_create(&new_state->o_thread_id, PATTR_DEFAULT, 
		oattr_main, (void *) new_state);

	return(0);
}

int
ocache_start()
{
	pthread_mutex_lock(&shared_mutex);
	search_active = 1;
	search_done = 0;
	pthread_cond_signal(&bg_active_cv);
	pthread_mutex_unlock(&shared_mutex);
	return (0);
}

int
ocache_stop(char *dir_path)
{
	printf("ocache_stop\n");
	pthread_mutex_lock(&shared_mutex);
	search_active = 0;
	search_done = 1;
	pthread_mutex_unlock(&shared_mutex);

	/* now we only has one cache table file. later we may want a cache table
	 * for each gid */
	ocache_write_file(dir_path);

	return (0);
}
