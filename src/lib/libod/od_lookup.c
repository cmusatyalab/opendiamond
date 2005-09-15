/*
 * 	Diamond (Release 1.0)
 *      A system for interactive brute-force search
 *
 *      Copyright (c) 2002-2005, Intel Corporation
 *      All Rights Reserved
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <rpc/rpc.h>
#include <assert.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "lib_tools.h"
#include "od.h"
#include "od_priv.h"
#include "lib_dconfig.h"



static char const cvsid[] = "$Header$";


/* number of buckets, must be power of 2 !! */
#define     DEV_HASH_BUCKETS    64

LIST_HEAD(devid_hash, od_srv)   ods_devid_hash[DEV_HASH_BUCKETS];

static int  done_init = 0;

/*
 * This is probably a very bad hash function.
 * XXX fix this later.
 */
static int
ods_id_hash(uint64_t devid)
{
	char *  cp;
	int     i;
	int     val = 0;

	cp = (char *)&devid;
	for (i=0; i < sizeof(devid); i++) {
		val ^= cp[i];
	}

	val = val & (DEV_HASH_BUCKETS - 1);
	return(val);
}



/*
 */
void
ods_init()
{
	int i;
	int     seed;
	int     fd;
	size_t  rbytes;
	assert(done_init == 0);
	done_init = 1;

	/* init the head of the hash buckets */
	for (i=0; i < DEV_HASH_BUCKETS; i++) {
		LIST_INIT(&ods_devid_hash[i]);
	}

	fd = open("/dev/random", O_RDONLY);
	assert(fd != -1);
	rbytes = read(fd, (void *)&seed, sizeof(seed));
	assert(rbytes == sizeof(seed));

	srand(seed);
}


static od_srv_t *
ods_dev_lookup(uint64_t devid)
{
	int         hash;
	od_srv_t *  osrv;

	hash = ods_id_hash(devid);

	LIST_FOREACH(osrv, &ods_devid_hash[hash], ods_id_link) {
		if (osrv->ods_id == devid) {
			return(osrv);
		}
	}
	return(NULL);
}

static void
ods_dev_insert(od_srv_t *osrv)
{
	int     hash;

	hash = ods_id_hash(osrv->ods_id);

	LIST_INSERT_HEAD(&ods_devid_hash[hash], osrv, ods_id_link);


}


od_srv_t *
ods_lookup_by_devid(uint64_t devid)
{
	od_srv_t *          osrv;
	uint32_t            haddr;
	struct hostent *    hent;
	struct in_addr      in;
	char *              tname;

	assert(done_init);

	osrv = ods_dev_lookup(devid);
	if (osrv != NULL) {
		return(osrv);
	}

	/*
	 * We don't have the connection, so set one up.
	 */

	osrv = (od_srv_t *)malloc(sizeof(*osrv));
	assert(osrv != NULL);

	osrv->ods_id = devid;

	haddr = (uint32_t)devid;

	hent = gethostbyaddr((char *)&haddr, sizeof(haddr), AF_INET);

	if (hent == NULL) {
		in.s_addr = haddr;
		tname = inet_ntoa(in);
		osrv->ods_name = strdup(tname);
		assert(osrv->ods_name != NULL);
	} else {
		osrv->ods_name = strdup(hent->h_name);
		assert(osrv->ods_name != NULL);
	}

	/* Open the client connection */
	osrv->ods_client = clnt_create(osrv->ods_name, MESSAGEPROG,
	                               MESSAGEVERS, "tcp");
	if (osrv->ods_client == NULL) {
		fprintf(stderr, "contacting %s", osrv->ods_name);
		clnt_pcreateerror("");
		free(osrv);
		return(NULL);
	}

	ods_dev_insert(osrv);

	return(osrv);
}


od_srv_t *
ods_lookup_by_oid(obj_id_t *oid)
{
	od_srv_t *          osrv;
	uint64_t            devid;

	assert(done_init);

	devid = oid->dev_id;

	osrv = ods_lookup_by_devid(devid);
	return(osrv);
}




#define MAX_HOSTS   64
od_srv_t *
ods_allocate_by_gid(groupid_t *gid)
{
	uint32_t    host_list[MAX_HOSTS];
	int         num_hosts;
	uint64_t    devid;
	od_srv_t *  osrv;
	int         err;
	int         idx;
	double      temp;

	assert(done_init);

	num_hosts = MAX_HOSTS;
	err = glkup_gid_hosts(*gid, &num_hosts, host_list);
	if (err == ENOENT) {
		fprintf(stderr, "group 0x%llx is not in gid map \n", *gid);
		assert(0);
		return(NULL);
	} else if (err == ENOMEM) {
		fprintf(stderr, "Too many hosts increase MAX_HOSTS and recompile \n");
		assert(0);
		return(NULL);
	} else if (err != 0) {
		fprintf(stderr, "Unknown error \n");
		assert(0);
		return(NULL);
	}


	temp = (double) rand();
	temp = ((double)num_hosts * temp);
	temp  = temp / (RAND_MAX + 1.0);

	idx = (int) temp;

	devid = host_list[idx];

	osrv = ods_lookup_by_devid(devid);
	return(osrv);
}

