#ifndef	_LOG_H_
#define	_LOG_H_

#define	SOCKET_DCTL_NAME	"/tmp/dctl_socket"


/*
 * This is the message header that is sent to
 * the host stub.
 */


typedef	enum {
	DCTL_OP_READ = 1,
	DCTL_OP_WRITE,
	DCTL_OP_LIST_NODES,
	DCTL_OP_LIST_LEAFS,
	DCTL_OP_REPLY,
} dctl_op_t;



typedef struct {
	dctl_op_t	dctl_op;
	uint32_t	dctl_err;
	uint32_t	dctl_dlen;
	uint32_t	dctl_plen;
} dctl_msg_hdr_t;



#endif /* !_LOG_H_ */
