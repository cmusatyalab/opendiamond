/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 3
 *
 *  Copyright (c) 2007 Carnegie Mellon University
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#include <dirent.h>
#include <fcntl.h>
#include <openssl/sha.h>
#include <pthread.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include "helper.h"

#define OBJHEXLEN (SHA_DIGEST_LENGTH*2) //*2 because bytes become 2 hex chars
#define GIDHEXLEN 16
#define SCRIPTNAME "distribute_objects.sh"

struct object_node {
  struct object_node *next;
  char old_filename[MAXPATHLEN];
  char new_filename[MAXPATHLEN];
};
typedef struct object_node object_node_t;

typedef struct {
  object_node_t *head;
  pthread_mutex_t mutex;
} object_list_t;

typedef struct {
  char hostname[MAXPATHLEN];
  char dataroot[MAXPATHLEN];
  char indexdir[MAXPATHLEN];
} content_server_t;

static void
free_list(object_list_t *list) {
  object_node_t *trav;

  if(list == NULL)
    return;

  if(list->head == NULL) {
    free(list);
    return;
  }

  trav = list->head;

  while(trav->next != NULL) {
    object_node_t *next = trav->next;
    free(trav);
    trav = next;
  }
  
  free(trav);
  return;
}


static int
check_ssh_cookies(int num_servers, char **hostname) {
  int i;

  if((num_servers < 1) || (hostname == NULL))
	return -1;

  for(i=0; i<num_servers; i++) {
	char command[NCARGS];
	snprintf(command, NCARGS, 
			 "ssh -o PasswordAuthentication=no -f %s exit > /dev/null 2>&1", 
			 hostname[i]);
	if(system(command) == 65280) { //65280 fail code found by empirical testing
	  fprintf(stderr, "There are no ssh cookies present for %s\n",
			  hostname[i]);
	  return -1;
	}
  }
  
  return 0;
}


/*
 * Recursive function which enumerates all objects located beneath a
 * certain directory.
 */

static int 
enumerate_recursive(object_list_t *list, char *path) {

  if(!list || !path)
    return -1;

  if(is_dir(path)) {
    DIR *dir;
    struct dirent *dirent;
    
    if((dir = opendir(path)) == NULL) {
      perror("opendir");
      return -1;
    }

    while((dirent = readdir(dir)) != NULL) {
      char newpath[MAXPATHLEN];
      
      if(!strcmp(dirent->d_name, ".") || !strcmp(dirent->d_name, ".."))
	continue;

      strncpy(newpath, path, MAXPATHLEN);
      strncat(newpath, "/", (MAXPATHLEN-strlen(newpath)));
      strncat(newpath, dirent->d_name, (MAXPATHLEN-1)-strlen(newpath));

      if(enumerate_recursive(list, newpath) < 0){
	closedir(dir);
	return -1;
      }
    }

    closedir(dir);
  }
  else if(is_file(path)) {
    object_node_t *obj;
    
    if((obj = (object_node_t *)malloc(sizeof(object_node_t))) == NULL) {
      perror("malloc");
      return -1;
    }

    strncpy(obj->old_filename, path, MAXPATHLEN);

    pthread_mutex_lock(&list->mutex);
    obj->next = list->head;
    list->head = obj;
    pthread_mutex_unlock(&list->mutex);
  }

  return 0;
}


/*
 * enumerate_all_objects 
 *
 * This function takes the filename of an index file which
 * lists all files and directories that will be part of a collection, and
 * expands it to each object in the collection.
 */

static object_list_t *
enumerate_all_objects(char *path) {
  FILE *fp;
  object_list_t *list;
  char buf[MAXPATHLEN];

  if(path == NULL)
    return NULL;

  if((fp = fopen(path, "r")) == NULL) {
    perror("fopen");
    return NULL;
  }

  if((list = (object_list_t *)malloc(sizeof(object_list_t))) == NULL) {
    perror("fopen");
    return NULL;
  }

  list->head = NULL;
  pthread_mutex_init(&list->mutex, NULL);

  while(fgets(buf, MAXPATHLEN, fp) != NULL) {
    int len;

    len = strlen(buf);
    if(buf[len-1] == '\n')
      buf[len-1] = '\0';

    if(enumerate_recursive(list, buf) < 0) {
      free_list(list);
      list = NULL;
      break;
    }
  }

  fclose(fp);

  return list;
}


static int 
get_content_server_config(content_server_t *conf) {
  char command[NCARGS];
  FILE *fp;
  int indexstat = 0, datastat = 0;

  if(conf == NULL) return -1;

  /* get the diamond_config file from the server */
  strcpy(command, "scp ");
  strcat(command, conf->hostname);
  strcat(command, ":~/.diamond/diamond_config /tmp/diamond_config > /dev/null");

  if(system(command) == -1) { //system() annoying if no ssh keys set up
    perror("system");
    return -1;
  }

  if((fp = fopen("/tmp/diamond_config", "r")) == NULL) {
    perror("fopen");
    return -1;
  }

  while(!indexstat || !datastat) {
    char buf[MAXPATHLEN];
    char *bufp;

    if(fgets(buf, MAXPATHLEN, fp) == NULL) {
      if(!datastat)
	fprintf(stderr, "couldn't find DATAROOT in %s's diamond_config\n",
		conf->hostname);
      if(!indexstat)
	fprintf(stderr, "couldn't find INDEXDIR in %s's diamond_config\n",
		conf->hostname);
      fclose(fp);
      return -1;
    }

    if((bufp = strtok(buf, " \n\t\r")) == NULL)
      continue;

    if(!strcmp(bufp, "DATAROOT")) {
      if((bufp = strtok(NULL, " \n\t\r")) == NULL) {
	fprintf(stderr, "DATAROOT formatted incorrectly in %s's"
		" diamond_config\n", conf->hostname);
	fclose(fp);
	return -1;
      }
      strncpy(conf->dataroot, bufp, MAXPATHLEN);
      datastat = 1;
    }
            
    if(!strcmp(buf, "INDEXDIR")) {
      if((bufp = strtok(NULL, " \n\t\r")) == NULL) {
	fprintf(stderr, "INDEXDIR formatted incorrectly in %s's"
		" diamond_config\n", conf->hostname);
	fclose(fp);
	return -1;
      }
      strncpy(conf->indexdir, bufp, MAXPATHLEN);
      indexstat = 1;
      continue;
    }
  }

  fclose(fp);

  return 0;
}



static int
generate_object_names(object_list_t *list) {
  object_node_t *trav;

  if((list == NULL) || (list->head == NULL))
    return -1;

  pthread_mutex_lock(&list->mutex);

  for(trav = list->head; trav != NULL; trav = trav->next) {
    int fd;
    unsigned char hash[SHA_DIGEST_LENGTH], *file;
    char *ext;
    struct stat buf;

    if(stat(trav->old_filename, &buf) < 0) {
      perror("stat");
      pthread_mutex_unlock(&list->mutex);
      return -1;
    }
    
    if((fd = open(trav->old_filename, O_RDONLY)) < 0) {
      perror("open");
      pthread_mutex_unlock(&list->mutex); 
      return -1;
    }
    
    if((file = mmap(0, buf.st_size, PROT_READ, MAP_SHARED, fd, 0)) 
       == MAP_FAILED) {
      perror("mmap");
      fprintf(stderr, "mmap failed on %s (%d bytes)\n", trav->old_filename,
	      (int)buf.st_size);
      pthread_mutex_unlock(&list->mutex);
      close(fd);
      return -1;
    }

    fprintf(stderr, ".");

    if(SHA1((const unsigned char *)file, buf.st_size, hash) == NULL) {
      fprintf(stderr, "SHA1() returned NULL!\n");
      pthread_mutex_unlock(&list->mutex);
      munmap(file, buf.st_size);
      close(fd);
      return -1;
    }
    
    if(binary_to_hex_string(OBJHEXLEN, trav->new_filename, MAXPATHLEN, 
			    hash, SHA_DIGEST_LENGTH) < 0) {
      fprintf(stderr, "Error converting hash to string!\n");
      pthread_mutex_unlock(&list->mutex);
      munmap(file, buf.st_size);
      close(fd);
      return -1;
    }


    /* Add the extension back onto the filename, which helps some programs
     * parse the contents of a file. */

    if((ext = parse_extension(trav->old_filename)) != NULL)
      strncat(trav->new_filename, ext, 
	      ((MAXPATHLEN - 1) - strlen(trav->new_filename)));

    munmap(file, buf.st_size);
    close(fd);
  }  

  fprintf(stderr, "\n");

  pthread_mutex_unlock(&list->mutex);
  return 0;
}


static int
generate_index_file(char *collection_name, object_list_t *list,
		    int num_servers, content_server_t *srv, char *gid) {
  unsigned long long *rdm;
  struct timeval tv;
  object_node_t *trav;
  char pathname[MAXPATHLEN];
  FILE **fp;
  int i;

  if((collection_name == NULL) || (list == NULL) || (srv == NULL) ||
     (num_servers < 1))
    return -1;

  if(gettimeofday(&tv, NULL) < 0) {
    perror("gettimeofday");
    return -1;
  }

  if((fp = (FILE **)malloc(num_servers * sizeof(FILE *))) == NULL) {
    perror("malloc");
    return -1;
  }

  if((rdm = (unsigned long long *)malloc(sizeof(unsigned long long))) == NULL) {
    perror("malloc");
    return -1;
  }

  /* Generate a random group ID. */

  srand(tv.tv_sec);
  *rdm = (((unsigned long long)rand()<<48) +
	  ((unsigned long long)rand()<<32) + 
	  ((unsigned long long)rand()<<16) + 
	  ((unsigned long long)rand()));
  if(binary_to_hex_string(GIDHEXLEN, gid, GIDHEXLEN+1, (unsigned char *)rdm, 
			  sizeof(unsigned long long)) < 0) {
    fprintf(stderr, "Failed creating a random group id.\n");
    return -1;
  }

  for(i=0; i<num_servers; i++) {
    snprintf(pathname, MAXPATHLEN, "GIDIDX%s-%s", gid, srv[i].hostname);
    if((fp[i] = fopen(pathname, "w+")) == NULL) {
      perror("fopen");
      return -1;
    }
  }

  for(trav = list->head, i=0; trav !=  NULL; trav = trav->next, i++) {
    char buf[MAXPATHLEN];
    i%=num_servers;
    snprintf(buf, MAXPATHLEN, "%s/%s", collection_name, trav->new_filename);
    if(escape_shell_chars(buf, MAXPATHLEN) < 0)
      fprintf(stderr, "failed escaping characters! script may have errors.\n");
    strcat(buf, "\n");
    fwrite(buf, strlen(buf), 1, fp[i]);
  }

  for(i=0; i<num_servers; i++)
    fclose(fp[i]);

  free(fp);

  return 0;
}


static int
write_distribution_script(char *collection_name, char *gid,
			  object_list_t *list, 
			  int num_servers, content_server_t *srv) {
  object_node_t *trav;  
  FILE *fp;
  int cur_server = 0, i;
  char command[NCARGS], cname[MAXPATHLEN];

  if((list == NULL) || (srv == NULL) || (num_servers < 1) || (gid == NULL))
    return -1;

  if((fp = fopen(SCRIPTNAME, "w+")) == NULL) {
    perror("fopen");
    return -1;
  }

  snprintf(cname, MAXPATHLEN, "%s", collection_name);
  if(escape_shell_chars(cname, MAXPATHLEN) < 0)
    fprintf(stderr, "failed escaping characters! script may have errors.\n");

  snprintf(command, NCARGS, "rm -rf /tmp/%s\n", cname);
  fwrite(command, strlen(command), 1, fp);

  /* make a directory for the collection on each server */
  for(i=0; i < num_servers; i++) {
    char hostname[MAXPATHLEN], dataroot[MAXPATHLEN];

    snprintf(hostname, MAXPATHLEN, "%s", srv[i].hostname);
    if(escape_shell_chars(hostname, MAXPATHLEN) < 0)
      fprintf(stderr, "failed escaping characters! script may have errors.\n");

    snprintf(dataroot, MAXPATHLEN, "%s", srv[i].dataroot);
    if(escape_shell_chars(dataroot, MAXPATHLEN) < 0)
      fprintf(stderr, "failed escaping characters! script may have errors.\n");

    snprintf(command, NCARGS, "mkdir -p /tmp/%s/%s/%s\n", cname, hostname,
	     cname);
    fwrite(command, strlen(command), 1, fp);

    snprintf(command, NCARGS, "echo \"mkdir -p %s/%s\" | ssh %s\n",
             dataroot, cname, hostname);
    fwrite(command, strlen(command), 1, fp);
  }

  
  /* copy a GID index file to each server. */
  for(i=0; i < num_servers; i++) {
    char hostname[MAXPATHLEN], indexdir[MAXPATHLEN];

    snprintf(hostname, MAXPATHLEN, "%s", srv[i].hostname);
    if(escape_shell_chars(hostname, MAXPATHLEN) < 0)
      fprintf(stderr, "failed escaping characters! script may have errors.\n");

    snprintf(indexdir, MAXPATHLEN, "%s", srv[i].indexdir);
    if(escape_shell_chars(indexdir, MAXPATHLEN) < 0)
      fprintf(stderr, "failed escaping characters! script may have errors.\n");

    snprintf(command, NCARGS, "scp -p GIDIDX%s-%s %s:%s/GIDIDX%s\n",
             gid, hostname, hostname, indexdir, gid);
    fwrite(command, strlen(command), 1, fp);
  }  

  for(trav = list->head; trav !=  NULL; 
      trav = trav->next, cur_server++) {
    char localdest[MAXPATHLEN], src[MAXPATHLEN], dest[MAXPATHLEN];

    cur_server %= num_servers;

    snprintf(localdest, MAXPATHLEN, "/tmp/%s/%s/%s/%s", collection_name, 
	     srv[cur_server].hostname, collection_name, trav->new_filename);
    if(escape_shell_chars(localdest, MAXPATHLEN) < 0)
      fprintf(stderr, "failed escaping characters! script may have errors.\n");

    snprintf(src, MAXPATHLEN, "%s", trav->old_filename);
    if(escape_shell_chars(src, MAXPATHLEN) < 0)
      fprintf(stderr, "failed escaping characters! script may have errors.\n");
    
    snprintf(dest, MAXPATHLEN, "%s:%s/%s/%s", srv[cur_server].hostname, 
	     srv[cur_server].dataroot, collection_name, trav->new_filename);
    if(escape_shell_chars(dest, MAXPATHLEN) < 0)
      fprintf(stderr, "failed escaping characters! script may have errors.\n");

    snprintf(command, NCARGS, "ln %s %s || scp -p %s %s\n", src, localdest, 
	     src, dest);
    fwrite(command, strlen(command), 1, fp);
  }


  /* copy the objects to each server. */
  for(i=0; i < num_servers; i++) {
    char src[MAXPATHLEN], dest[MAXPATHLEN];

    snprintf(src, MAXPATHLEN, "/tmp/%s/%s/%s", collection_name, 
	     srv[i].hostname, collection_name);
    if(escape_shell_chars(src, MAXPATHLEN) < 0)
      fprintf(stderr, "failed escaping characters! script may have errors.\n");

    snprintf(dest, MAXPATHLEN, "%s:%s", srv[i].hostname, srv[i].dataroot); 
    if(escape_shell_chars(dest, MAXPATHLEN) < 0)
      fprintf(stderr, "failed escaping characters! script may have errors.\n");

    snprintf(command, NCARGS, "rsync -r %s %s\n", src, dest);
    fwrite(command, strlen(command), 1, fp);
  }  
  
  /* Set mode bits to 0755 */

  if(chmod(SCRIPTNAME, 
	   S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH) < 0) {
    perror("chmod");
    fclose(fp);
    return -1;
  }

  fclose(fp);

  return 0;
}


static int 
callback(void *NotUsed, int argc, char **argv, char **azColName){
  int i;

  for(i=0; i<argc; i++)
    printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
  printf("\n");

  return 0;
}


static int
store_provenance(char *collection_name, char *gid,
		 object_list_t *list, int num_servers, content_server_t *srv) {
  sqlite3 *db = NULL;
  char *errmsg, *timep;
  char command[NCARGS], dbname[MAXPATHLEN], localhost[MAXPATHLEN];
  char gidstr[MAXPATHLEN];
  object_node_t *trav;  
  int cur_server = 0, i;
  uid_t uid;
  time_t tm;

  if(time(&tm) < (time_t)0) {
    perror("time");
    return -1;
  }
  
  if((timep = ctime(&tm)) == NULL) {
    perror("ctime");
    return -1;
  }

  uid = getuid();

  if(gethostname(localhost, MAXPATHLEN) < 0) {
    perror("gethostname");
    return -1;
  }

  if(insert_gid_colons(gidstr, gid) < 0) {
    fprintf(stderr, "Error inserting colons into group ID string.\n");
    return -1;
  }

  snprintf(dbname, MAXPATHLEN, "provenance-%s.db", collection_name);

  if((sqlite3_open(dbname, &db) != SQLITE_OK) || (db == NULL)) {
    fprintf(stderr, "Error opening new SQLite DB %s: %s\n", 
	    dbname, sqlite3_errmsg(db));
    return -1;
  }

  if(sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS objects "
		  "( groupid TEXT, server TEXT, oldpath TEXT,"
		  " newpath TEXT );", callback, 0, &errmsg) != SQLITE_OK) {
    fprintf(stderr, "SQL error: %s\n", errmsg);
    sqlite3_free(errmsg);
    sqlite3_close(db);
    return -1;
  }

  for(trav = list->head; trav !=  NULL; trav = trav->next, cur_server++) {
    char *hnam, dest[MAXPATHLEN];
    cur_server %= num_servers;
    
    snprintf(dest, MAXPATHLEN, "%s/%s/%s", srv[cur_server].dataroot,
	     collection_name, trav->new_filename);
    
    hnam = strip_username(srv[cur_server].hostname);
    if(hnam == NULL) hnam = srv[cur_server].hostname;
    
    snprintf(command, NCARGS, 
	     "INSERT INTO objects VALUES (\"%s\", \"%s\", \"%s\", \"%s\");",
	     gidstr, hnam, trav->old_filename, dest);
    
    if(sqlite3_exec(db, command, callback, 0, &errmsg) != SQLITE_OK) {
      fprintf(stderr, "SQL error: %s\n", errmsg);
      sqlite3_free(errmsg);
      sqlite3_close(db);
      return -1;
    }
  }

  if(sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS collections "
		  "( collection TEXT, groupid TEXT, "
		  "localhost TEXT, uid INTEGER, time TEXT, server TEXT );", 
		  callback, 0, &errmsg) != SQLITE_OK) {
    fprintf(stderr, "SQL error: %s\n", errmsg);
    sqlite3_free(errmsg);
    sqlite3_close(db);
    return -1;
  }

  for(i=0; i<num_servers; i++) {
    char *hnam;

    hnam = strip_username(srv[i].hostname);
    if(hnam == NULL) hnam = srv[i].hostname;
    
    snprintf(command, NCARGS, 
	     "INSERT INTO collections VALUES "
	     "(\"%s\", \"%s\", \"%s\", \"%d\", \"%s\", \"%s\" );",
	     collection_name, gidstr, localhost, uid, timep, hnam);
    
    if(sqlite3_exec(db, command, callback, 0, &errmsg) != SQLITE_OK) {
      fprintf(stderr, "SQL error: %s\n", errmsg);
      sqlite3_free(errmsg);
      sqlite3_close(db);
      return -1;
    }
  }

  sqlite3_close(db);
  return 0;
}


static int
store_metadata(char *collection_name, char *gid,
	       int num_servers, content_server_t *srv) {
  sqlite3 *db = NULL;
  char *errmsg, command[NCARGS], dbname[MAXPATHLEN], gidstr[MAXPATHLEN];
  int cur_server;

  if(insert_gid_colons(gidstr, gid) < 0) {
    fprintf(stderr, "Error inserting colons into group ID string.\n");
    return -1;
  }

  snprintf(dbname, MAXPATHLEN, "metadata-%s.db", collection_name);

  if((sqlite3_open(dbname, &db) != SQLITE_OK) || (db == NULL)) {
    fprintf(stderr, "Error opening new SQLite DB %s: %s\n", 
	    dbname, sqlite3_errmsg(db));
    return -1;
  }

  if(sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS metadata "
		  "( collection TEXT, groupid TEXT, server TEXT );", 
		  callback, 0, &errmsg) != SQLITE_OK) {
    fprintf(stderr, "SQL error: %s\n", errmsg);
    sqlite3_free(errmsg);
    sqlite3_close(db);
    return -1;
  }

  for(cur_server = 0; cur_server < num_servers; cur_server++) {
    char *hnam;

    hnam = strip_username(srv[cur_server].hostname);
    if(hnam == NULL) hnam = srv[cur_server].hostname;
   
    snprintf(command, NCARGS, 
	     "INSERT INTO metadata VALUES (\"%s\", \"%s\", \"%s\");",
	     collection_name, gidstr, hnam);

    if(sqlite3_exec(db, command, callback, 0, &errmsg) != SQLITE_OK) {
      fprintf(stderr, "SQL error: %s\n", errmsg);
      sqlite3_free(errmsg);
      sqlite3_close(db);
      return -1;
    }
  }

  sqlite3_close(db);
  return 0;
}


static int
create_collection(char *name, char *listfile, int num_servers, 
		  char **hostname) {
  object_list_t *list;
  content_server_t *srv;
  int i;
  char gid[GIDHEXLEN+1];

  if((srv = (content_server_t *)malloc(num_servers * sizeof(content_server_t))) == NULL) {
    perror("malloc");
    return -1;
  }

  if(check_ssh_cookies(num_servers, hostname) < 0) {
	fprintf(stderr, "You need ssh cookies set up to run volcano.\n");
	return -1;
  }

  for(i=0; i<num_servers; i++) {
    strncpy(srv[i].hostname, hostname[i], MAXPATHLEN);
    if(get_content_server_config(&srv[i]) < 0) {
      fprintf(stderr, "Error reading config file from server: %s\n", 
	      hostname[i]);
      return -1;
    }
  }

  if((list = enumerate_all_objects(listfile)) == NULL) {
    fprintf(stderr, "Error enumerating list file: %s\n", listfile);
    return -1;
  }
  
  if(generate_object_names(list) < 0) {
    fprintf(stderr, "Error generating object names\n");
    free_list(list);
    return -1;
  }

  if(generate_index_file(name, list, num_servers, srv, gid) < 0) {
    fprintf(stderr, "Error generating object index file\n");
    free_list(list);
    return -1;
  }

  if(write_distribution_script(name, gid, list, num_servers, srv) < 0) {
    fprintf(stderr, "Error writing distribution script\n");
    free_list(list);
    return -1;
  }

  if(store_provenance(name, gid, list, num_servers, srv) < 0) {
    fprintf(stderr, "Error storing provenance in SQLite DB\n");
    free_list(list);
    return -1;
  }

  if(store_metadata(name, gid, num_servers, srv) < 0) {
    fprintf(stderr, "Error storing metadata information in SQLite DB\n");
    free_list(list);
    return -1;
  }


  //create new SQLite database file for provenance
  //create new SQLite database file for scoping
  //clean up
  
  free_list(list);

  return 0;
}

static void usage_create(void) {
  fprintf(stderr, "usage: volcano create <collection-name> <index-file> "
                  "<content-server-1> [content-server-2]...\n");
}

static void usage_list(void) {
  //TODO
}

static void usage_remove(void) {
  //TODO
}

static void usage(void) {
  fprintf(stderr, "usage: volcano <command> <parameters>\n");
  fprintf(stderr, "Try volcano --help for help.\n");
}

static void help(void) {
  fprintf(stderr, "usage: volcano <command> <parameters>\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "The only supported command at the moment is:\n"
	  "  create \tCreate a new collection from a list of objects");
  fprintf(stderr, "\n");
  fprintf(stderr, "Try volcano <command> --help for command-specific help.\n");
}

int main(int argc, char *argv[]) {
  char *command = argv[1];

  if((argc < 2)) {
    usage();
    exit(EXIT_FAILURE);
  }
  
  if(!strcmp(command, "--help")) {
    help();
    exit(EXIT_SUCCESS);
  }
  else if(!strcmp(command, "create")) {
    int num_servers;
    char *collname, *listfile, **hostname;

    if((argc < 5)) {
      usage_create();
      exit(EXIT_FAILURE);
    }

    num_servers = argc - 4;
    if(num_servers <= 0) {
      fprintf(stderr, "No servers specified\n");
      usage_create();
      exit(EXIT_FAILURE);
    }

    collname = argv[2];
    listfile = argv[3];
    hostname = &argv[4];

    if(create_collection(collname, listfile, num_servers, hostname) < 0)
	  exit(EXIT_FAILURE);
  }
  else if(!strcmp(command, "remove")) {
    //TODO
  }
  else if(!strcmp(command, "list")) {
    //TODO
  }
  else if(!strcmp(command, "remove")) {
    usage();
    exit(EXIT_FAILURE);
  }

  exit(EXIT_SUCCESS);
}
