#ifndef _SOCKET_TRANS_H_
#define _SOCKET_TRANS_H_


/* some random port numbers that we are  using ??? */
#define		CONTROL_PORT		5872
#define		DATA_PORT	    	5873
#define		LOG_PORT		    5874


#define	CNTL_CMD_START			1
#define	CNTL_CMD_STOP			2
#define	CNTL_CMD_SET_SEARCHLET	3
#define	CNTL_CMD_SET_LIST		4
#define	CNTL_CMD_TERMINATE		5
#define	CNTL_CMD_TERM_DONE		6
#define	CNTL_CMD_GET_STATS		7
#define	CNTL_CMD_RET_STATS		8
#define	CNTL_CMD_GET_CHAR		9
#define	CNTL_CMD_RET_CHAR		10
#define	CNTL_CMD_SETLOG			11
#define	CNTL_CMD_WRITE_LEAF 	12
#define	CNTL_CMD_READ_LEAF 	    13
#define	CNTL_CMD_LIST_NODES     14
#define	CNTL_CMD_LIST_LEAFS     15
#define	CNTL_CMD_WLEAF_DONE 	16
#define	CNTL_CMD_RLEAF_DONE     17
#define	CNTL_CMD_LNODES_DONE    18
#define	CNTL_CMD_LLEAFS_DONE    19
#define	CNTL_CMD_ADD_GID        20
#define	CNTL_CMD_CLEAR_GIDS     21
#define	CNTL_CMD_SET_BLOB 		22
#define	CNTL_CMD_SET_OFFLOAD	23


/*
 * This is the header that is place on all control
 * messages.
 */

typedef struct control_header {
	uint32_t	generation_number;
	uint32_t	command;
	uint32_t	data_len;
	uint32_t	spare;
} control_header_t;


/*
 * For a set searchlet command this header will be after
 * the control header.  This tells how much data is allocated
 * to each chunck of data.  The data itself is stored after
 * this sub header.  Once trick to note is that we make sure
 * that the filter data starts on a multiple of 4 bytes, so
 * the starting offset is the spec_size rounded up to the next 
 * boundary.
 */
typedef struct searchlet_subhead {
	uint32_t	spec_len;
	uint32_t	filter_len;
} searchlet_subhead_t;

/*
 * This body of the message for returning the device
 * characteristics.
 */

typedef struct devchar_subhead {
	uint32_t	dcs_isa;		
	uint64_t	dcs_speed;
	uint64_t	dcs_mem;
} devchar_subhead_t;


#define	OBJ_MAGIC_HEADER	0x54124566
typedef struct object_header {
	uint32_t	obj_magic;	/* for debugging */
	uint32_t	attr_len;	/* length of the attributes */
	uint32_t	data_len;	/* length of the data portion */
	uint32_t	version_num;	/* search version number */
}  obj_header_t;



typedef struct fstats_subheader {
	char		fss_name[MAX_FILTER_NAME];
	uint32_t	fss_objs_processed;
	uint32_t	fss_objs_dropped;
	uint64_t	fss_avg_exec_time;
} fstats_subheader_t;

typedef struct sgid_subheader {
	uint64_t	sgid_gid;
} sgid_subheader_t;


typedef struct dstats_subheader {
	uint32_t	dss_total_objs;
	uint32_t	dss_objs_proc;
	uint32_t	dss_objs_drop;
	uint32_t	dss_objs_bp;
	uint32_t	dss_system_load;
	uint64_t	dss_avg_obj_time;
	uint32_t	dss_num_filters;
} dstats_subheader_t;

typedef struct {
	uint32_t	log_level;	/* the level flags */
	uint32_t	log_src;	/* the source flags */ 
}  setlog_subheader_t;



typedef struct {
	uint32_t	dctl_err;       /* error code on this request */
    int32_t     dctl_opid;      /* opid to match request/response */
    uint32_t    dctl_plen;      /* len of path in data (include termination) */
    uint32_t    dctl_dlen;      /* len of data (include termination) */
    uint32_t    dctl_dtype;     /* len of data (include termination) */
	char	    dctl_data[0];	/* the len of the data */
} dctl_subheader_t;


typedef struct {
    uint32_t    blob_nlen;      /* len of the name data (including null) */
    uint32_t    blob_blen;      /* then length of the blob of the name data */
	char	    blob_sdata[0];	/* the data with the name followed by blob */
} blob_subheader_t;

typedef struct {
    uint64_t    offl_data;      /* host cycles to apply to this problem */
} offload_subheader_t;



/*
 * Header that goes on the log buffers that are sent to the host.
 */
#define	LOG_MAGIC_HEADER	0x54122756

typedef struct log_header {
	uint32_t	log_magic;	/* for debugging */
	uint32_t	log_len;	/* length log data */
}  log_header_t;


/*
 * credit count state that is passed in the reverse direction
 * on the credit count channel.
 */
#define	CC_MAGIC_HEADER		0x81204570
typedef struct credit_count_msg {
	uint32_t	cc_magic;	/* for debugging */
	uint32_t	cc_count;	/* number of object can send */
} credit_count_msg_t;


#endif /* _SOCKET_TRANS_H_ */
