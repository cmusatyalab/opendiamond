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

#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/time.h>
#include <netdb.h>
#include "ring.h"
#include "diamond_consts.h"
#include "diamond_types.h"
#include "obj_attr.h"
#include "lib_odisk.h"
#include "socket_trans.h"
#include "lib_dctl.h"
#include "lib_hstub.h"
#include "lib_log.h"


int
handle_new_obj(void *hcookie, obj_data_t *odata, int vno)
{

	printf("new_obj!! - vno  %d \n", vno);
	return(0);
}

void
handle_log_data(void *hcookie, char *data, int len, int dev)
{
	printf("have log data \n");

}

int
main(int argc, char **argv)
{
	int	err;
	void *	cookie;
	void *	log_cookie;
	void *	dctl_cookie;
	hstub_cb_args_t		cb_args;
	struct in_addr		addr;


	cb_args.log_data_cb = handle_log_data;

	err = inet_aton("127.0.0.1", &addr);

	log_init(&log_cookie);
	dctl_init(&dctl_cookie);

	/* XXX */
	cookie = device_init(100, addr.s_addr, 0, &cb_args, dctl_cookie,
	                     log_cookie);


	err = device_start(cookie, 101);
	if (err) {
		printf("failed to start device \n");
	}

	err = device_set_searchlet(cookie, 102, "/tmp/x", "/tmp/y" );

	printf("XXX sending stop \n");
	err = device_stop(cookie, 102);
	if (err) {
		printf("failed to stop device \n");
	}

	printf("sleeping \n");
	sleep(10);
	printf("done sleep \n");

	exit(0);
}
