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
#include "lib_odisk.h"
#include "odisk_priv.h"
#include "rtimer.h"



int
main(int argc, char **argv)
{
	odisk_state_t*	odisk;
	obj_data_t *	new_obj;
	char *			path;
	int		err;

	if (argc > 1) {
			path = argv[1];
	} else {
			path = "/opt/dir1";
	}

	err = odisk_init(&odisk, path);
	if (err) {
		errno = err;
		perror("failed to init odisk");
		exit(1);
	}

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
	exit(0);
}
