#ifndef	_LOG_H_
#define	_LOG_H_

#define	SOCKET_LOG_NAME		"/tmp/log_socket"


/*
 * This is the message header that is sent to
 * the host stub.
 */

typedef	enum {
	LOG_SOURCE_BACKGROUND = 1,
	LOG_SOURCE_DEVICE
} source_type_t;


typedef struct {
	int		log_len;
	source_type_t	log_type;
	int		dev_id;
} log_msg_t;




#endif /* !_LOG_H_ */


