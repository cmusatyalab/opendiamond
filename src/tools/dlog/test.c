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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>

#define	MAX_BUFFER	4096
#define	SOCK_NAME	"/tmp/sock"

	
int
main(int argc, char **argv)
{
	int	fd, newsock;
	char 	data[MAX_BUFFER];
	struct sockaddr_un sa;
	struct sockaddr_un newaddr;
	int	err;
	int	len;
	int	slen;



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
