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



/*
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
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
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>


static char const cvsid[] = "$Header$";

#define	MAX_BUFFER	4096
#define	SOCK_NAME	"/tmp/sock"


int
main(int argc, char **argv)
{
	int		fd, newsock;
	char 		data[MAX_BUFFER];
	struct sockaddr_un sa;
	struct sockaddr_un newaddr;
	int		err;
	int		len;
	socklen_t	slen;



	fd = socket(PF_UNIX, SOCK_STREAM, 0);



	strcpy(sa.sun_path, SOCK_NAME);
	sa.sun_family = AF_UNIX;
	unlink(sa.sun_path);


	err = bind(fd, (struct sockaddr *)&sa, sizeof (sa));
	if (err < 0) {
		perror("connect failed ");
		exit(1);
	}

	if (listen(fd, 5) == -1) {
		perror("listen failed ");
		exit(1);

	}


	while (1) {
		slen = sizeof(newaddr);
		if ((newsock = accept(fd, (struct sockaddr *)&newaddr, &slen))
		    == -1) {

			perror("accept failed \n");
			exit(1);
		}


		len = send(newsock, data, MAX_BUFFER, 0);
		if (len < 0) {
			perror("send failed \n");
			exit(1);
		} else {
			printf("send worked \n");
			sleep(20);
		}
	}
}
