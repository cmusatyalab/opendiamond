/*
 * These file handles a lot of the device specific code.  For the current
 * version we have state for each of the devices.
 */
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/time.h>
#include <netdb.h>
#include <string.h>
#include <assert.h>
#include "ring.h"
#include "obj_attr.h"
#include "lib_odisk.h"
#include "lib_searchlet.h"
#include "socket_trans.h"
#include "lib_sstub.h"
#include "sstub_impl.h"


void
sstub_write_log(listener_state_t *lstate, cstate_t *cstate)
{
	printf("write log \n");
	/* clear the flags for now, there is no more data */
	pthread_mutex_lock(&cstate->cmutex);
	cstate->flags &= ~CSTATE_LOG_DATA;
	pthread_mutex_unlock(&cstate->cmutex);
	return;
}




void
sstub_except_log(listener_state_t *lstate, cstate_t *cstate)
{

	/* Handle the case where we are shutting down */
	if (cstate->flags & CSTATE_SHUTTING_DOWN) {
		return;
	}

	printf("except log \n");
	return;
}



void
sstub_read_log(listener_state_t *lstate, cstate_t *cstate)
{
	/* Handle the case where we are shutting down */
	if (cstate->flags & CSTATE_SHUTTING_DOWN) {
		return;
	}
	printf("sstub_read_log \n");
	return;

}


