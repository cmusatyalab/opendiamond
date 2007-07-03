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

#include <unistd.h>
#include <errno.h>
#include "socket_trans.h"

/*
 * Read "n" bytes from a descriptor reliably. 
 */
ssize_t
readn(int fd, void *vptr, size_t n)
{
  size_t  nleft;
  ssize_t nread;
  char   *ptr;

  ptr = vptr;
  nleft = n;

  while (nleft > 0) {
    if ( (nread = read(fd, ptr, nleft)) < 0) {
      perror("read");
      if (errno == EINTR)
        nread = 0;      /* and call read() again */
      else
        return (-1);
    } else if (nread == 0)
      break;              /* EOF */

    nleft -= nread;
    ptr += nread;
  }
  return (n - nleft);         /* return >= 0 */
}


/*
 * Write "n" bytes to a descriptor reliably. 
 */
ssize_t                        
writen(int fd, const void *vptr, size_t n)
{
  size_t nleft;
  ssize_t nwritten;
  const char *ptr;

  ptr = vptr;
  nleft = n;
  while (nleft > 0) {
    if ( (nwritten = write(fd, ptr, nleft)) <= 0) {
      if (nwritten < 0 && errno == EINTR)
        nwritten = 0;   /* and call write() again */
      else
        return (-1);    /* error */
    }

    nleft -= nwritten;
    ptr += nwritten;
  }
  return (n);
}


char *
diamond_error(diamond_rc_t *rc) {
	static char buf[128];

	if(rc == NULL)
	  return NULL;

	switch(rc->service_err) {
	case DIAMOND_SUCCESS:
	  sprintf(buf, "RPC call succeeded.");
	  break;
	case DIAMOND_FAILURE:
	  sprintf(buf,"RPC call failed generically.");
	  break;  
	case DIAMOND_NOMEM:
	  sprintf(buf, "RPC call failed from an out-of-memory error.");
	  break;
	case DIAMOND_FAILEDSYSCALL:
	  sprintf(buf, "RPC call failed from a failed system call: %s", 
		  strerror(rc->opcode_err));
	  break;
	case DIAMOND_OPERR:
	  sprintf(buf, "RPC call failed from an opcode-specific error.");
	  break;
	}

	return buf;
}
