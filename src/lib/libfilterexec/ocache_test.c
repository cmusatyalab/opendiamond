#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <stdint.h>
#include "lib_od.h"
#include "lib_ocache.h"
#include "ocache_priv.h"
//#include "obj_attr.h"

int
main(int argc, char **argv)
{
	ocache_state_t*	ocache;
	void *		log_cookie;
	void *		dctl_cookie;
	int		err;
	char fsig[16]={"ANIMOWLOEMOKLWML"};
	char iattr_sig[16]={"VNIMOWLOEMOKLWML"};
	char oattr_sig[16]={"BNIMOWLOEMOKLWML"};
	unsigned char signature[16];

	log_init(&log_cookie);
	dctl_init(&dctl_cookie);

	//err = ocache_init(&ocache, "/opt/dir1", dctl_cookie, log_cookie);
	err = ocache_init("/opt/dir1", dctl_cookie, log_cookie);
	if (err) {
		errno = err;
		perror("failed to init ocache");
		exit(1);
	}

	ocache_start();

	printf("cache test done \n");
	exit(0);
}
