#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <stdint.h>
#include <ctype.h>
#include "lib_od.h"
#include "lib_odisk.h"
#include "odisk_priv.h"

int
rebuild_idx(odisk_state_t *odisk)
{
	int		err;

	err = odisk_clear_indexes(odisk);
	if (err) {
		errno = err;
		perror("Failed to clear indexes \n");
		exit(1);
	}

	err = odisk_build_indexes(odisk);
	if (err) {
		errno = err;
		perror("Failed to build indexes \n");
		exit(1);
	}
	return(0);
}

uint64_t 
parse_uint64_string(const char* s) {
  int i, o;
  unsigned int x;	// Will actually hold an unsigned char
  uint64_t u = 0u;

  /*
  sscanf(s, "%llx", &u);
  printf("parsed gid is 0x%llx\n", u);
  return u;
  */

  assert(s);
  //fprintf(stderr, "parse_uint64_string s = %s\n", s);
  for (i=0; i<8; i++) {
    o = 3*i;
    assert(isxdigit(s[o]) && isxdigit(s[o+1]));
    assert( (s[o+2] == ':') || (s[o+2] == '\0') );
    sscanf(s+o, "%2x", &x);
    u <<= 8;
    u += x;
  }
  // printf("parsed uint64_t is 0x%llx\n", u);
  return u;
}





int
main(int argc, char **argv)
{
	odisk_state_t*	odisk;
	FILE * 		cur_file;
	char		idx_file[256];
	char		path_name[256];
	char		attr_name[256];
	char *		path = "/opt/dir1";
	gid_idx_ent_t	gid_ent;
	uint64_t	gid;
	int		err, num;

	if (argc < 2) {
		exit(1);
	}

	gid = parse_uint64_string(argv[1]);

	err = odisk_init(&odisk, path);
	if (err) {
		errno = err;
		perror("failed to init odisk");
		exit(1);
	}
	

	sprintf(idx_file, "%s/%s%016llX", path, GID_IDX, gid);
	cur_file = fopen(idx_file, "r");
	if (cur_file == NULL) {
		fprintf(stderr, "unable to open idx %s \n", idx_file);
	}

	while (cur_file != NULL) {
		num = fread(&gid_ent, sizeof(gid_ent), 1, cur_file);
		if (num == 1) {
			sprintf(path_name, "%s/%s", path, gid_ent.gid_name);
			err = remove(path_name);
			if (err != 0) {
				perror("remove failed \n");
				exit(1);
			}
			sprintf(attr_name, "%s%s", path_name, ATTR_EXT);
			err = remove(attr_name);
			if (err != 0) {
				perror("attr remove failed \n");
				exit(1);
			}
		} else {
			cur_file = NULL;
		}
	}


	rebuild_idx(odisk);

	exit(0);
}
