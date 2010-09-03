/*
 * miniRPC - TCP RPC library with asynchronous operations
 *
 * Copyright (C) 2007 Carnegie Mellon University
 *
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU Lesser General Public License version 2.1, as shown in
 * the file COPYING.
 */

#include <rpc/types.h>
#include <rpc/xdr.h>
#define MINIRPC_INTERNAL
#include "internal.h"

/* This is a fake XDR stream which exists to count the bytes in a serialized
   structure.  We don't support reading from the stream, and we don't actually
   write to the stream because we have no backing store. */

static bool_t getlong(XDR *xdrs, long *lp)
{
	return FALSE;
}

static bool_t putlong(XDR *xdrs, const long *lp)
{
	/* Apparently "long" doesn't really mean "long". */
	xdrs->x_handy += 4;
	return TRUE;
}

static bool_t getbytes(XDR *xdrs, caddr_t addr, u_int len)
{
	return FALSE;
}

static bool_t putbytes(XDR *xdrs, const char *addr, u_int len)
{
	xdrs->x_handy += len;
	return TRUE;
}

static u_int getpos(const XDR *xdrs)
{
	return xdrs->x_handy;
}

static bool_t setpos(XDR *xdrs, u_int pos)
{
	xdrs->x_handy=pos;
	return TRUE;
}

/* inline is an optimization which lets the caller directly access our buffer.
   We don't want to provide some fake buffer for them to write into, so we
   refuse to support the operation. */
static int32_t *doinline(XDR *xdrs, u_int len)
{
	return NULL;
}

static void destroy(XDR *xdrs)
{
	return;
}

static bool_t getint32(XDR *xdrs, int32_t *ip)
{
	return FALSE;
}

static bool_t putint32(XDR *xdrs, const int32_t *ip)
{
	xdrs->x_handy += 4;
	return TRUE;
}

static struct xdr_ops ops = {
	.x_getlong = getlong,
	.x_putlong = putlong,
	.x_getbytes = getbytes,
	.x_putbytes = putbytes,
	.x_getpostn = getpos,
	.x_setpostn = setpos,
	.x_inline = doinline,
	.x_destroy = destroy,
	.x_getint32 = getint32,
	.x_putint32 = putint32
};

void xdrlen_create(XDR *xdrs)
{
	xdrs->x_op=XDR_ENCODE;
	xdrs->x_ops=&ops;
	xdrs->x_handy=0;
}
