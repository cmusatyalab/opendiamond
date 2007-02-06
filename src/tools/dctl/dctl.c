/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 2
 *
 *  Copyright (c) 2002-2005 Intel Corporation
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <string.h>
#include <assert.h>
#include "lib_tools.h"
#include "lib_dctl.h"
#include "dctl_socket.h"


static char const cvsid[] = "$Header$";


int
sock_read_reply(int fd, dctl_msg_hdr_t *msg, int *len, char *data)
{
	int 	rlen;
	int	    dlen;
	char *	buf;


	rlen = recv(fd, msg, sizeof(*msg), MSG_WAITALL);
	if (rlen != sizeof(*msg)) {
		printf("XXX failed to rx reply \n");
		return(EINVAL); /* XXX */
	}


	dlen = msg->dctl_dlen;
	if (dlen == 0) {
		*len = 0;
		return(msg->dctl_err);
	}

	/*
	 * See if we can read into this buffer, if so
	 * get the payload.
	 */
	if (dlen <= *len) {
		rlen = recv(fd, data, dlen, MSG_WAITALL);
		if (rlen != dlen) {
			return(EINVAL); /* XXX */
		}

		*len = dlen;
		return(msg->dctl_err);
	} else {
		/*
		 * If we get here, the caller didn't provide enough
		 * storage to handle the response.  We need to still
		 * pull the data out of the socket, or we will get
		 * out of synce and be in trouble for the next
		 * call.
		 */
		assert(0);
		buf = (char *)malloc(dlen);
		assert(buf!=NULL);

		rlen = recv(fd, buf, dlen, MSG_WAITALL);
		if (rlen != sizeof(*msg)) {
			return(EINVAL); /* XXX */
		}
		*len = dlen;
		return(ENOMEM);
	}
}



int
sock_read_leaf(int fd, dctl_data_type_t *dtype, char *path,
               int *len, char *data)
{
	dctl_msg_hdr_t	msg;
	int				path_len;
	int				err;
	int				slen;

	path_len = strlen(path) + 1;

	msg.dctl_dlen = path_len;
	msg.dctl_plen = path_len;
	msg.dctl_op = DCTL_OP_READ;

	/* send the header */
	slen = send(fd, &msg, sizeof(msg), 0);
	if (slen != sizeof(msg)) {
		return(EINVAL);	/* XXX */
	}

	/* send the body */
	slen = send(fd, path, path_len, 0);
	if (slen != path_len) {
		return(EINVAL);	/* XXX */
	}

	/*
	 * Now we need to wait for the response.
	 *
	 */
	err = sock_read_reply(fd, &msg, len, data);
	if (err == 0) {
		*dtype = msg.dctl_dtype;
	}
	return(err);
}


int
sock_list_leafs(int fd, char *path, int *num_ents, dctl_entry_t *entry_space)
{
	dctl_msg_hdr_t	msg;
	int				path_len;
	int				err;
	int				data_len;
	int				slen;

	path_len = strlen(path) + 1;

	msg.dctl_dlen = path_len;
	msg.dctl_plen = path_len;
	msg.dctl_op = DCTL_OP_LIST_LEAFS;

	/* send the header */
	slen = send(fd, &msg, sizeof(msg), 0);
	if (slen != sizeof(msg)) {
		return(EINVAL);	/* XXX */
	}

	/* send the body */
	slen = send(fd, path, path_len, 0);
	if (slen != path_len) {
		return(EINVAL);	/* XXX */
	}

	/*
	 * Now we need to wait for the response.
	 *
	 */
	data_len = *num_ents * sizeof(dctl_entry_t);
	err = sock_read_reply(fd, &msg, &data_len, (char *)entry_space);

	if ((err == 0) || (err == ENOMEM)) {
		/* convert from bytes to entries */
		*num_ents = data_len / sizeof(dctl_entry_t);
	}

	return(err);
}

int
sock_list_nodes(int fd, char *path, int *num_ents, dctl_entry_t *entry_space)
{
	dctl_msg_hdr_t	msg;
	int		path_len;
	int		err;
	int		data_len;
	int		slen;

	path_len = strlen(path) + 1;

	msg.dctl_dlen = path_len;
	msg.dctl_plen = path_len;
	msg.dctl_op = DCTL_OP_LIST_NODES;

	/* send the header */
	slen = send(fd, &msg, sizeof(msg), 0);
	if (slen != sizeof(msg)) {
		return(EINVAL);	/* XXX */
	}

	/* send the body */
	slen = send(fd, path, path_len, 0);
	if (slen!= path_len) {
		return(EINVAL);	/* XXX */
	}

	/*
	 * Now we need to wait for the response.
	 *
	 */
	data_len = *num_ents * sizeof(dctl_entry_t);
	err = sock_read_reply(fd, &msg, &data_len, (char *)entry_space);
	if ((err == 0) || (err == ENOMEM)) {
		/* convert from bytes to entries */
		*num_ents = data_len / sizeof(dctl_entry_t);
	}
	return(err);
}


int
sock_write_leaf(int fd, char *path, int len, char *data)
{
	dctl_msg_hdr_t	msg;
	int		path_len;
	int		err;
	int		data_len;
	int		slen;

	path_len = strlen(path) + 1;

	msg.dctl_dlen = path_len + len;
	msg.dctl_plen = path_len;
	msg.dctl_op = DCTL_OP_WRITE;

	/* send the header */
	slen = send(fd, &msg, sizeof(msg), 0);
	if (slen != sizeof(msg)) {
		return(EINVAL);	/* XXX */
	}

	/* send the body */
	slen = send(fd, path, path_len, 0);
	if (slen!= path_len) {
		return(EINVAL);	/* XXX */
	}


	/* send the body */
	slen = send(fd, data, len, 0);
	if (slen!= len) {
		return(EINVAL);	/* XXX */
	}

	/*
	 * Now we need to wait for the response.
	 */
	data_len = 0;
	err = sock_read_reply(fd, &msg, &data_len, NULL);
	if (err == 0) {
		err = msg.dctl_err;
	}
	return(err);
}



void
show_leaf(int fd, char *leaf_path, int name)
{
	char                databuf[64];
	int		            len;
	dctl_data_type_t    dtype;
	int                 err, i;

	len = 64;
	err = sock_read_leaf(fd, &dtype, leaf_path, &len, (char *)databuf);
	if (err) {
		assert(0);
		exit(1);
	}
	if (name) {
		fprintf(stdout, "%s = ", leaf_path);
	}

	switch (dtype) {
		case DCTL_DT_UINT32:
			fprintf(stdout, "%d\n", *(uint32_t *)databuf);
			break;

		case DCTL_DT_UINT64:
			fprintf(stdout, "%lld\n", *(uint64_t *)databuf);
			break;

		case DCTL_DT_STRING:
			fprintf(stdout, "%s\n", databuf);
			break;

		case DCTL_DT_CHAR:
			fprintf(stdout, "%c\n", databuf[0]);
			break;

		default:
			/* we don't know what it is, so dump it in hex */
			for(i=0; i < len; i++) {
				fprintf(stdout, "%02x", databuf[i]);

			}
			fprintf(stdout, "\n");
			break;
	}
}




#define	MAX_ENTS	128
void
dump_node(int fd, char *cur_path, int name, int nodes)
{
	dctl_entry_t	data[MAX_ENTS];
	int		ents;
	int		err, i;
	char	new_path[256];

	/*
	 * first all the leafs.
	 */
	ents = MAX_ENTS;
	err = sock_list_leafs(fd, cur_path, &ents, data);
	if (err) {
		exit(1);
	}

	for (i = 0; i < ents; i++) {
		sprintf(new_path, "%s.%s", cur_path, data[i].entry_name);
		show_leaf(fd, new_path, name);
	}

	ents = MAX_ENTS;
	err = sock_list_nodes(fd, cur_path, &ents, data);
	assert(err == 0);

	for (i = 0; i < ents; i++) {
		if (strlen(cur_path) == 0) {
			sprintf(new_path, "%s", data[i].entry_name);
		} else {
			sprintf(new_path, "%s.%s", cur_path, data[i].entry_name);
		}
		if ((nodes) && (name)) {
			fprintf(stdout, "%s.\n", new_path);
		}
		dump_node(fd, new_path, name, nodes);
	}
}

#define MAX_PATH    256
#define MAX_VALSTR    256
int
write_values(int fd, char *pathval)
{
	char                path[MAX_PATH];
	char                val[MAX_VALSTR];
	dctl_data_type_t    dtype;
	char *              idx;
	int                 len;
	int                 err;


	idx = index(pathval, '=');
	if (idx == NULL) {
		return(EINVAL);
	}

	len = idx - pathval;

	assert(len < (MAX_PATH -1));
	memcpy(path, pathval, len);
	path[len] = '\0';

	/* look up this value and determine its type */
	len = MAX_VALSTR;
	err = sock_read_leaf(fd, &dtype, path, &len, val);
	if (err) {
		fprintf(stderr, "node %s doesn't exist \n", path);
		return(EINVAL);
	}

	idx++;
	switch (dtype) {
		case DCTL_DT_UINT32:
			*(uint32_t *)val = atoi(idx);
			len = sizeof(uint32_t);
			break;

		case DCTL_DT_UINT64:
			*(uint64_t *)val = atoll(idx);
			len = sizeof(uint64_t);
			break;

		case DCTL_DT_STRING:
			len = strlen(idx) + 1;
			memcpy(val, idx, len);
			break;

		case DCTL_DT_CHAR:
			*val = *idx;
			len = sizeof(char);
			break;

		default:
			/* we don't know what it is, so dump it in hex */
			fprintf(stderr, "unknown data type on %s  \n", path);
			return(EINVAL);
			break;

	}

	err = sock_write_leaf(fd, path, len, val);
	return(err);
}



void
dump_values(int fd, char *path, int recurse, int name, int nodes)
{
	/*
	 * If recurse is not set, then this should be a leaf,
	 * otherwise this should be a node.
	 */

	if (recurse == 0) {
		show_leaf(fd, path, name);
	} else {
		dump_node(fd, path, name, nodes);
	}
}



void
usage()
{
	fprintf(stdout, "dctl [-r] [-i interval] [-n] variable ... \n");
	fprintf(stdout, "dctl -w variable=value  ... \n");
	fprintf(stdout, "\t -h show this message \n");
	fprintf(stdout, "\t -i repeate the operation at interval seconds \n");
	fprintf(stdout, "\t -n suppress names before each variable \n");
	fprintf(stdout, "\t -p include node names in the output \n");
	fprintf(stdout, "\t -r recursively dump everything below variable \n");
	fprintf(stdout, "\t -w set the variables to specified values \n");
}



int
main(int argc, char **argv)
{
	int	    fd;
	struct  sockaddr_un sa;
	int     write = 0;
	int     interval = 0;
	int     recurse = 0;
	int     name = 1;
	int     nodes = 0;
	int	    err;
	int	    c, i;

	/*
	 * The command line options.
	 */
	while (1) {
		c = getopt(argc, argv, "hi:nprw");
		if (c == -1) {
			break;
		}

		switch (c) {

			case 'h':
				usage();
				exit(0);
				break;

			case 'i':
				interval = atoi(optarg);
				break;

			case 'n':
				name = 0;
				break;

			case 'p':
				nodes = 1;
				break;


			case 'r':
				recurse = 1;
				break;

			case 'w':
				write = 1;
				break;

			default:
				fprintf(stderr, "unknown option %c\n", c);
				usage();
				exit(1);
				break;
		}
	}


	/*
	 * This is the main loop.  We just keep trying to open the socket
	 * and the log contents while the socket is valid.
	 *
	 * TODO:  This doesn't work correctly if you have more than
	 * one search going on a machine at the same time.
	 */

	while (1) {
		char	user_name[MAX_USER_NAME];

		fd = socket(PF_UNIX, SOCK_STREAM, 0);

		/* bind the socket to a path name */
		get_user_name(user_name);
		sprintf(sa.sun_path, "%s.%s", SOCKET_DCTL_NAME, user_name);


		sa.sun_family = AF_UNIX;


		err = connect(fd, (struct sockaddr *)&sa, sizeof (sa));
		if (err < 0) {
			/*
			 * If the open fails, then nobody is
			 * running, so we sleep for a while
			 * and retry later.
			 */
			sleep(1);
		} else {

			if (write) {
				for (i=optind; i < argc; i++) {
					err = write_values(fd, argv[i]);
					if (err) {
						/* XXX */
						exit(1);
					}
				}
			} else {
				if (optind == argc) {
					dump_values(fd, "", recurse, name, nodes);
				} else {
					for (i=optind; i < argc; i++) {
						dump_values(fd, argv[i], recurse, name, nodes);
					}
				}
			}

			/* if the interval is 0, then we are done,
			 * otherwise we sleep and continue.
			 */
			if (interval == 0) {
				break;
			} else {
				sleep(interval);
			}
		}
		close(fd);
	}

	exit(0);
}


