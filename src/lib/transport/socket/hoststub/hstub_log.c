/*
 * These file handles a lot of the device specific code.  For the current
 * version we have state for each of the devices.
 */
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
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
#include <assert.h>
#include "ring.h"
#include "lib_searchlet.h"
#include "socket_trans.h"
#include "obj_attr.h"
#include "lib_odisk.h"
#include "lib_hstub.h"
#include "hstub_impl.h"


void
hstub_read_log(sdevice_state_t *dev)
{
	printf("read log\n");	
}

void
hstub_except_log(sdevice_state_t * dev)
{
	printf("except_log \n");	
}


void
hstub_write_log(sdevice_state_t * dev)
{
	printf("write_log \n");	
}


