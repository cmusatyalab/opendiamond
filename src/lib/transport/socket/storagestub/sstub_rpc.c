/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 3
 *
 *  Copyright (c) 2007 Carnegie Mellon University
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
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
#include "diamond_consts.h"
#include "diamond_types.h"
#include "lib_tools.h"
#include "obj_attr.h"
#include "lib_odisk.h"
#include "socket_trans.h"
#include "lib_dctl.h"
#include "lib_sstub.h"
#include "sstub_impl.h"

void
sstub_except_rpc(listener_state_t * lstate, cstate_t * cstate)
{
  printf("XXX except_rpc \n");
  /*
   * Handle the case where we are shutting down 
   */
  if (cstate->flags & CSTATE_SHUTTING_DOWN) {
    return;
  }

  return;
}

/*
 * This function is called when we see that the Sun RPC server has
 * written bytes back for a client to read.  We forward the bytes from
 * the Sun RPC connection to the control connection.
 */

void
sstub_read_rpc(listener_state_t * lstate, cstate_t * cstate)
{
  int size_in, size_out;
  char buf[4096];

  /*
   * Handle the case where we are shutting down 
   */
  if (cstate->flags & CSTATE_SHUTTING_DOWN) {
    return;
  }
  
  /* Attempt to process up to 4096 bytes of data in this iteration. */
  
  size_in = read(cstate->rpc_fd, (void *)buf, 4096);
  if(size_in < 0){
    perror("read");
    return;
  }
  else if(size_in == 0) { /* EOF */
    close(cstate->control_fd);
    fprintf(stderr, "(tunnel) The Sun RPC server closed its connection\n");
    exit(EXIT_SUCCESS);	  
  }
  
  size_out = writen(cstate->control_fd, (void *)buf, size_in);
  if(size_out < 0) {
    perror("write");
    return;
  }
	
  if(size_in != size_out) {
    fprintf(stderr, "Somehow lost bytes, from %d in to %d out!\n", 
	    size_in, size_out);
    return;
  }

  return;
}
