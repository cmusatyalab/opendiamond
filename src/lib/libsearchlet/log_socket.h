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


typedef enum {
	LOG_SETLEVEL_ALL = 1,
	LOG_SETLEVEL_DEVICE,
	LOG_SETLEVEL_HOST
} log_level_op_t;


typedef struct {
	log_level_op_t	log_op;		/* the op in host order */
	uint32_t	log_level;	/* the level in network order */	
	uint32_t	log_src;	/* the source flags in network order */	
	uint32_t	dev_id;		/* device id for LOG_SETLEVEL_HOST */
} log_set_level_t;


#endif /* !_LOG_H_ */
