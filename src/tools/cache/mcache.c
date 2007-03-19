/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 2
 *
 *  Copyright (c) 2002-2007 Intel Corporation
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <dirent.h>
#include <sys/param.h>

#include <glib.h>

#include "diamond_types.h"
#include "lib_tools.h"
#include "lib_dconfig.h"
#include "lib_odisk.h"
#include "lib_filterexec.h"
#include "lib_ocache.h"

#include "centry.h"

static char const cvsid[] = "$Header$";

/*
 * Merge Diamond caches
 */

#define CACHE_EXT ".CACHEFL"
#define DEFAULT_OUTPUT_DIR  "cache_merged"
#define BLOCKSIZE 8192

char *new_cache_dir = DEFAULT_OUTPUT_DIR;


void usage()
{
	fprintf(stdout, "mcache [-h] [-d new-cache-dir] list-of-cache-directories\n");
}

void objectIterator(gpointer key, gpointer value, 
					gpointer user_data) 
{
	/* key = object sig, value = cache entry */
	cache_obj *cobj = (cache_obj *) value;
	int fd = *((int *) user_data);
	put_cache_entry(fd, cobj);
}


void filterIterator(gpointer key, gpointer value, 
					gpointer user_data) 
{
	char fpath[PATH_MAX];
	int fd;
	int rc;
	
	/* key = filter sig, value = object hash table */
	GHashTable *oHash = (GHashTable *) value;
	
	/* make the associated directory */
	sprintf(fpath, "%s/%s", new_cache_dir, (char *) key);
	rc = mkdir(fpath, S_IRWXU);
	if (rc < 0) {
		printf("Error %d directory %s\n", errno, fpath);
	}
	
	/* create a new CACHEFL file */
	sprintf(fpath, "%s/%s%s", new_cache_dir, 
			(char *) key, CACHE_EXT);	
	fd = open(fpath, O_CREAT | O_RDWR | O_TRUNC, 00777);
	if (fd < 0) {
		printf("Error %d creating cache file %s\n",
				errno, fpath);
		exit(1);
	}
	
	printf("Writing %s\n", fpath);
	g_hash_table_foreach(oHash, objectIterator, &fd);
	close(fd);
}

void attrIterator(gpointer key, gpointer value, 
					gpointer user_data) 
{
	/* key = object sig, value = cache init entry */
	cache_init_obj *cobj = (cache_init_obj *) value;
	int fd = *((int *) user_data);
	put_cache_init_entry(fd, cobj);
}


char *entryToKey(cache_obj * cobj) 
{
	char *keyString;
	char *oSigString;
	char *aSigString;
	
	keyString = (char *) malloc(4 * SIG_SIZE + 1);
	
	oSigString = sig_string(&cobj->id_sig);
	aSigString = sig_string(&cobj->iattr_sig);
	strcpy(keyString, oSigString);
	strcat(keyString, aSigString);
	free(oSigString);
	free(aSigString);
	
	return keyString;
}

int copy_file(char *frompath, char *topath) 
{
	char *databuf = NULL;
	int fromfd;
	int tofd;
	int fn;
	int tn;
	int rv = 0;
	
	fromfd = open(frompath, O_RDONLY);
	if (fromfd < 0) {
		printf("Error %d opening %s\n", errno, frompath);
		return(1);
	}
		
	tofd = open(topath, O_CREAT | O_RDWR | O_TRUNC);	
	if (tofd < 0) {
		printf("Error %d creating %s\n", errno, topath);
		return(1);
	}
	
	databuf = (char *) malloc(BLOCKSIZE);
	fn = BLOCKSIZE;
	while (fn > 0) {
		fn = read(fromfd, databuf, BLOCKSIZE);
		if (fn == 0) 
			break;   /* done */
		if (fn < 0) {
			printf("Error %d reading from %s\n",
					errno, frompath);
			rv = 1;
			break;
		}
		tn = write(tofd, databuf, fn);
		if (tn < 0) {
			printf("Error %d writing to %s\n",
					errno, topath);
			rv = 1;
			break;
		}
	}		
	free(databuf);
	
	return(rv);
}

void copy_dir(char *cache_dir, char *dir_name) 
{
	char path[PATH_MAX];
	char *oldpath;
	char *newpath;
	DIR *dir;		
	struct dirent *cur_ent;
	struct stat buf;
	int rc;

	/* does destination directory exist? */	
	snprintf(path, MAXPATHLEN, "%s/%s", new_cache_dir, dir_name);
	rc = stat(path, &buf);
	if (rc < 0) {
		if (errno == ENOENT) {
			printf("Creating directory %s\n", path);
			rc = mkdir(path, S_IRWXU);
			if (rc < 0) {
				printf("Error %d creating directory %s\n", 
						errno, path);
				exit(1);
			}
		} else {
			printf("Error %d getting status on %s\n", 
						errno, path);
			exit(1);
		}
	} else {
		/* make sure it's a diretory */
		if (!S_ISDIR(buf.st_mode)) {
			printf("Error: %s is not a directory\n", path);
			exit(1);
		}
	}
	
	newpath = (char *) malloc(PATH_MAX);
	oldpath = (char *) malloc(PATH_MAX);

	/* copy over non-duplicate entries */
	snprintf(path, MAXPATHLEN, "%s/%s", cache_dir, dir_name);
	printf("Copying %s\n", path);
	dir = opendir(path);
	while ((cur_ent = readdir(dir)) != NULL) {
		if (strcmp(cur_ent->d_name, ".") == 0)
			continue;
		if (strcmp(cur_ent->d_name, "..") == 0)
			continue;
			
		/* does this file exist in the new directory? */
		snprintf(newpath, MAXPATHLEN, "%s/%s/%s", new_cache_dir, 
					dir_name, cur_ent->d_name);
		rc = stat(newpath, &buf);
		if (rc < 0 && errno == ENOENT) {
			/* copy the file */
			snprintf(oldpath, MAXPATHLEN, "%s/%s/%s", cache_dir,
						dir_name, cur_ent->d_name);
			printf("Copying %s to %s\n", path, newpath);
			rc = copy_file(oldpath, newpath);
			if (rc != 0) {
				printf("Error copying %s to %s\n", 
						oldpath, newpath);
			}
		} else {
			printf("Entry %s already exists\n", newpath);
		}
	}
	
	free(oldpath);
	free(newpath);
}

int main(int argc, char **argv)
{
	int c;
	extern char *optarg;
	char *cache_dir;
	GHashTable *filter_hash;
	GHashTable *oHash;
	GHashTable *attrHash;
	char filter_name[MAXNAMLEN];
	char *filter_key;
	char path[PATH_MAX];
	DIR *dir;		
	struct dirent *cur_ent;
	int extlen;
	int flen;
	char *poss_ext;
	char *oName;
	char *suffix;
	int fd;
	cache_obj *cobj;
	cache_obj *tobj;
	cache_init_obj *ciobj;
	cache_init_obj *aobj;
	int rc;
	int i;
	
	if (argc < 3) {
		usage();
		exit(0);
	}
	
	/*
	 * The command line options.
	 */
	while (1) {
		c = getopt(argc, argv, "hd:");
		if (c == -1) {
			break;
		}

		switch (c) {
			case 'd':
				new_cache_dir = optarg;
				break;

			case 'h':
				usage();
				exit(0);
				break;

			default:
				printf("unknown option %c\n", c);
				usage();
				exit(1);
				break;
		}
	}
	
	/* try to create the output directory */
	rc = mkdir(new_cache_dir, S_IRWXU);
	if (rc < 0) {
		printf("Error %d creating output directory %s\n", 
				errno, new_cache_dir);
		exit(1);
	}
	
	attrHash = g_hash_table_new(g_str_hash, g_str_equal);
	filter_hash = g_hash_table_new(g_str_hash, g_str_equal);

	for (i = 1; i < argc; i++) {
		cache_dir = argv[i];
		
		/* skip args */
		if (cache_dir[0] == '-')
			continue;
		
		printf("Processing cache directory %s\n", cache_dir);

		/* copy over files from subdirectories */
		copy_dir(cache_dir, "binary");
		copy_dir(cache_dir, "blobs");
		copy_dir(cache_dir, "filters");
		copy_dir(cache_dir, "specs");

		/* merge ATTRSIG file */
		snprintf(path, MAXPATHLEN, "%s/ATTRSIG", cache_dir);
		fd = open(path, O_RDONLY);
		if (fd == -1) {
			printf("Could not open %s: %d\n", path, errno);
			continue;
		}

		printf("Processing ATTRSIG\n");
		while ((ciobj = get_cache_init_entry(fd))) {
			oName = sig_string(&ciobj->id_sig);
			/* look up object, merge if exists */                                 
			aobj = g_hash_table_lookup(attrHash, oName);
			if (aobj == NULL) {
				printf("Adding object %s\n", 
						sig_string(&ciobj->id_sig));
				g_hash_table_insert(attrHash, oName, ciobj);
				aobj = ciobj;
			} else {
				printf("Found object %s\n", 
						sig_string(&ciobj->id_sig));
				/* merge entries */
				if (ciobj->attr.entry_num != 
					aobj->attr.entry_num) {
					printf("Warning: attribute number mismatch on %s\n", 
							oName);
				}
			}
		}
		
		/* process the CACHEFL files */
		dir = opendir(cache_dir);
		while ((cur_ent = readdir(dir)) != NULL) {
	
			/* check for cache file extension */
			extlen = strlen(CACHE_EXT);
			flen = strlen(cur_ent->d_name);
			if (flen > extlen) {
				poss_ext = &cur_ent->d_name[flen - extlen];
				if (strcmp(poss_ext, CACHE_EXT) != 0) {
					continue;
				}
			} else {
				continue;
			}
	
			snprintf(path, MAXPATHLEN, "%s/%s", cache_dir, cur_ent->d_name);
			fd = open(path, O_RDONLY);
			if (fd == -1) {
				printf("Could not open %s: %d\n", path, errno);
				continue;
			}
			
			printf("Processing %s\n", path);
			
			strcpy(filter_name, cur_ent->d_name);
			suffix = index(filter_name, '.');
			*suffix = '\0';
			
			/* look up filter, add an object hash if absent */
			oHash = g_hash_table_lookup(filter_hash, filter_name);
			if (oHash == NULL) {
				printf("Adding table for %s\n", filter_name);
				filter_key = (char *) malloc(strlen(filter_name)+1);
				strcpy(filter_key, filter_name);
				oHash = g_hash_table_new(g_str_hash, g_str_equal);
				g_hash_table_insert(filter_hash, filter_key, oHash);
			}
 
			while ((cobj = get_cache_entry(fd))) {
				oName = entryToKey(cobj);
				/* look up object, merge if exists */                                 
				tobj = g_hash_table_lookup(oHash, oName);
				if (tobj == NULL) {
					printf("Adding object %s\n", 
							sig_string(&cobj->id_sig));
					g_hash_table_insert(oHash, oName, cobj);
					tobj = cobj;
				} else {
					printf("Found object %s\n", 
							sig_string(&cobj->id_sig));
					/* merge entries */
					if (!sig_match(&cobj->iattr_sig, 
									&tobj->iattr_sig)) {
						printf("Warning: input attr sig mismatch on %s\n", 
								oName);
					}
					if (tobj->result != cobj->result) {
						printf("Warning: result mismatch on %s\n", 
								oName);
					}
					tobj->eval_count += cobj->eval_count;
					tobj->hit_count += cobj->hit_count;
					/* keep qid, exec mode from first entry */
					printf("New evals %d, hits %d\n",
							tobj->eval_count, tobj->hit_count);
					free(cobj);
				}
			}  
			close(fd);
		}	
		
		closedir(dir);
		
	}
	
	/* now write the merged versions */
	printf("\nWriting ATTRSIG\n");
	sprintf(path, "%s/ATTRSIG", new_cache_dir);
	fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 00777);
	if (fd < 0) {
		printf("Error %d opening %s\n", errno, path);
		exit(1);
	}
	
	g_hash_table_foreach(attrHash, attrIterator, &fd);
	close(fd);
	
	g_hash_table_foreach(filter_hash, filterIterator, NULL);
	
	exit(0);
}
