#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "lib_od.h"
#include "lib_odisk.h"
#include "lib_log.h"
#include "lib_dctl.h"
#include "odisk_priv.h"
#include "rtimer.h"


uint32_t
get_devid(char * host_addr)
{
	int	err;
	struct in_addr	in;

	err = inet_aton(host_addr, &in);

	return(in.s_addr);
}


int
main(int argc, char **argv)
{
	odisk_state_t*	odisk;
	char *			path;
	int		err;
	uint32_t	devid;
	void *		log_cookie;
	void *		dctl_cookie;
	char *		host_addr;


	host_addr = argv[1];
	
	if (argc > 2) {
			path = argv[2];
	} else {
			path = "/opt/dir1";
	}

	log_init(&log_cookie);
	dctl_init(&dctl_cookie);

	err = odisk_init(&odisk, path, dctl_cookie, log_cookie);
	if (err) {
		errno = err;
		perror("failed to init odisk");
		exit(1);
	}

	devid = get_devid(host_addr);
	printf("dev_id %08x \n", devid);

	odisk_write_oids(odisk, devid);

	exit(0);
}
