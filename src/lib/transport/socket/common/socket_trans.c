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

#define SERVICE_BUFSIZ 128
#define OPCODE_BUFSIZ 128

char *
diamond_error(diamond_rc_t *rc) {
	static char retbuf[SERVICE_BUFSIZ+OPCODE_BUFSIZ];
	char servbuf[SERVICE_BUFSIZ];
	char opbuf[OPCODE_BUFSIZ];

	if(rc == NULL)
	  return NULL;

	servbuf[0] = '\0';

 	switch(rc->service_err) {
	case DIAMOND_SUCCESS:
	  snprintf(servbuf, SERVICE_BUFSIZ, "RPC call succeeded.");
	  break;
	case DIAMOND_FAILURE:
	  snprintf(servbuf, SERVICE_BUFSIZ, "RPC call failed generically.");
	  break;
	case DIAMOND_NOMEM:
	  snprintf(servbuf, SERVICE_BUFSIZ, "RPC call failed from an "
		   "out-of-memory error.");
	  break;
	case DIAMOND_FAILEDSYSCALL:
	  snprintf(servbuf, SERVICE_BUFSIZ, "RPC call failed from a failed "
		   "system call: %s", strerror(rc->opcode_err));
	  break;
	case DIAMOND_OPERR:
	  snprintf(servbuf, SERVICE_BUFSIZ, "RPC call failed from an "
		   "call-specific error: ");
	  break;
	default:
	  snprintf(servbuf, SERVICE_BUFSIZ, "RPC call failed with an unknown "
		   "error code.");
	}

	opbuf[0] = '\0';

	switch(rc->opcode_err) {
	case DIAMOND_OPCODE_FAILURE:
	  snprintf(opbuf, OPCODE_BUFSIZ, "Generic failure.");
	  break;
	case DIAMOND_OPCODE_FCACHEMISS:
	  snprintf(opbuf, OPCODE_BUFSIZ, "The filter signature was not "
		   "found in the cache.");
	  break;
	default:
	  break;
	}

	strncpy(retbuf, servbuf, SERVICE_BUFSIZ);
	strncat(retbuf, opbuf, OPCODE_BUFSIZ);

	return retbuf;
}
