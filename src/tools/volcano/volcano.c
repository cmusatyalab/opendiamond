12345678901234567890123456789012345678901234567890123456789012345678901234567890

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  char dataroot[MAXPATHLEN];
  char indexdir[MAXPATHLEN];
} content_handle;

int get_content_server_config(char *hostname, content_handle *conf) {
  char command[NCARGS];
  FILE *fp;
  int indexstat = 0, datastat = 0;

  if(conf == NULL) return -1;

  /* get the diamond_config file from the server */
  strcpy(command, "scp ");
  strcat(command, hostname);
  strcat(command, ":~/.diamond/diamond_config /tmp/diamond_config");
  if(system(command) == -1) { //very annoying if you don't have ssh keys set up
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
      fclose(fp);
      return -1;
    }

    for(bufp = buf; *bufp != ' ' && *bufp != '\0'; bufp++) continue;
    if(*bufp == '\0') continue;

    *bufp = '\0';
    if(!strcmp(buf, "DATAROOT")) {
      bufp++;
      strcpy(conf->dataroot, bufp);
      status--;
      datastat = 1;
    }
            
    if(!strcmp(buf, "INDEXDIR")) {
      bufp++;
      strcpy(conf->indexdir, bufp);
      indexstat = 1;
      continue;
    }
  }

  return 0;
}


int place_object(char *collection_name, char *listfile, char *scopename,
                      int num_servers, char **contentname) {
  FILE *listfp;
  char command[NCARGS];
  char objects[MAXPATHLEN];
  int serverno, i, rem = NCARGS;
  content_handle conf[];

  if((conf = malloc(sizeof(content_handle)*num_servers)) == NULL) {
    perror("malloc");
    return -1;
  }

  if((listfp = fopen(listfile, "r")) == NULL) {
    perror("fopen");
    return -1;
  }

  /* make a directory for the collection on each server */
  for(i=0; i<num_servers; i++) {
    snprintf(command, NCARGS, "echo \"mkdir %s:%s/%s\" | ssh %s",
             hostname[serverno], conf[serverno].dataroot, collection_name);

    if(system(command) == -1) { //very annoying if you don't have ssh keys set up
      perror("system");
      goto failure;
    }
  }

  serverno = 0;

  while(fgets(objects, MAXPATHLEN, listfp) != NULL) {
    int err;

    serverno %= num_servers;

    snprintf(command, NCARGS, "scp -p %s %s:%s", objects, hostname[serverno],
             conf[serverno].dataroot)

    if(system(command) == -1) { //very annoying if you don't have ssh keys set up
      perror("system");
      goto failure;
    }

    serverno++;
  }

  

  fclose(listfp);

  return 0;

failure:
  fclose(listfp);
  free(conf);
  return -1;
}

void usage_create(void) {
  fprintf(stderr, "usage: volcano create [collection-name] [object-list] "
                  "[scope-server] [content-server-1] ...\n");
  fprintf(stderr, "Try volcano --help for help.\n");
}

void usage_list(void) {
  //TODO
}

void usage_remove(void) {
  //TODO
}

void usage(void) {
  fprintf(stderr, "usage: volcano [command] [options]\n");
  fprintf(stderr, "Try volcano --help for help.\n");
}


int main(int argc, char *argv[]) {
  char *command;

  if(argc < 2) {
    usage();
    exit(EXIT_FAILURE);
  }

  command = argv[1];
  if(!strcmp(command, "create")) {
    int num_servers;
    char *collname, *listfile, **hostname;

    num_servers = argc - 4;
    if(num_servers <= 0) {
      fprintf(stderr, "no servers specified\n");
      usage_create();
      exit(EXIT_FAILURE);
    }

    collname = argv[2];
    listfile = argv[3];
    hostname = &argv[4];

    if(create_collection(collname, listfile, num_servers, hostname) < 0) {
	fprintf(stderr, "failed creating collection\n");
	exit(EXIT_FAILURE);
    }
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