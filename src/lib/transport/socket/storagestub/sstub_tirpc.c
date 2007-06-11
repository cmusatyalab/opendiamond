/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 2
 *
 *  Copyright (c) 2007 Carnegie Mellon University
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */


void
sstub_except_tirpc(listener_state_t * lstate, cstate_t * cstate)
{
  printf("XXX except_tirpc \n");
  /*
   * Handle the case where we are shutting down 
   */
  if (cstate->flags & CSTATE_SHUTTING_DOWN) {
    return;
  }

  return;
}

/*
 * This function is called when we see that the TI-RPC server has
 * written bytes back for a client to read.  We forward the bytes from
 * the TI-RPC connection to the control connection.
 */

void
sstub_read_tirpc(listener_state_t * lstate, cstate_t * cstate)
{
  int size_in, size_out, error;
  char buf[4096];

  /*
   * Handle the case where we are shutting down 
   */
  if (cstate->flags & CSTATE_SHUTTING_DOWN) {
    return;
  }
  
  /* Attempt to process up to 4096 bytes of data in this iteration. */
  
  size_in = read(cstate->tirpc_fd, (void *)buf, 4096);
  if(size_in < 0){
    perror("read");
    return;
  }
  
  size_out = write(cstate->control_fd, (void *)buf, size_in);
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
