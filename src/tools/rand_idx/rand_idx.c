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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "lib_od.h"
#include "lib_odisk.h"
#include "odisk_priv.h"


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
get_rand(int max)
{
	int	new;
	int	rv;
	double	val;

	rv = (double)rand();

	val = rv /((double) RAND_MAX);
	val *= max;

	new = (int)val;
	assert(new >= 0);
	assert(new < max);
	return(new);
}

void
usage()
{
	fprintf(stdout, "rand_stat -g <gid> \n");
	fprintf(stdout, "\t-g <gid> gid of the group to modify \n");
}



int
main(int argc, char **argv)
{
	FILE * 		cur_file;
	char		idx_file[256];
	char *		path = "/opt/dir1";
	int			have_gid = 0;
	gid_idx_ent_t	gid_ent;
	uint64_t	gid = 0;
	int			err, num;
	int			i,c;
	struct	stat		my_stat;
	int			j,k;
	int			ssize;
	int			nstat;
	gid_idx_ent_t *		stat_buffer;
	extern char *	optarg;

	/*
	 * The command line options.
	 */
	while (1) {
		c = getopt(argc, argv, "g:h");
		if (c == -1) {
			break;
		}

		switch (c) {
			case 'h':
				usage();
				exit(0);
				break;


			case 'g':
				gid = parse_uint64_string(optarg);
				have_gid = 1;
				break;
	
			default:
				printf("unknown option %c\n", c);
				break;
		}
	}
	
	if (have_gid == 0) {
		usage();
		exit(1);
	}

	sprintf(idx_file, "%s/%s%016llX", path, GID_IDX, gid);
	err = stat(idx_file, &my_stat);
	if (err) {
		perror("stat index file");
		exit(1);
	}

	cur_file = fopen(idx_file, "r");
	if (cur_file == NULL) {
		fprintf(stderr, "unable to open idx %s \n", idx_file);
	}

	ssize = my_stat.st_size;

	stat_buffer = (gid_idx_ent_t *)malloc(ssize);
	nstat = ssize/sizeof(gid_ent);
	printf("nstat %d \n", nstat);

	assert(stat_buffer != NULL);

	num = fread(stat_buffer, ssize, 1, cur_file);
	if (num != 1) {
		
		printf("failed to read state file: %d -> %d \n", num, ssize);
		exit(1);		
	}
	fclose(cur_file);


	for (i=0; i < (nstat * 100); i++) {
		j = get_rand(nstat);
		k = get_rand(nstat);
		if (j==k) continue;

		gid_ent = stat_buffer[j];
		stat_buffer[j] = stat_buffer[k];
		stat_buffer[k] = gid_ent;
	}

	cur_file = fopen(idx_file, "w");
	if (cur_file == NULL) {
		fprintf(stderr, "unable to open idx %s \n", idx_file);
	}


	num = fwrite(stat_buffer, ssize, 1, cur_file);
	if (num != 1) {
		printf("failed to write state file \n");
		exit(1);		
	} 

	fclose(cur_file);

	exit(0);
}
