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
#include "lib_dctl.h"
#include <assert.h>
#include "dctl.h"



int
sock_read_reply(int fd, dctl_msg_hdr_t *msg, int *len, char *data)
{
	int 	rlen;
	int	dlen;
	char *	buf;

	rlen = recv(fd, msg, sizeof(*msg), MSG_WAITALL);
	if (rlen != sizeof(*msg)) {
		return(EINVAL); /* XXX */
	}

	dlen = msg->dctl_dlen;
	if (dlen == 0) {
		*len = 0;
		return(0);
	}

	/*
	 * See if we can read into this buffer, if so
	 * get the payload.
	 */
	if (dlen <= *len) {
		rlen = recv(fd, data, dlen, MSG_WAITALL);
		if (rlen != dlen) {
			printf("bad read on data \n");
			return(EINVAL); /* XXX */
		}

		*len = dlen;
		return(0);	
	} else {
		/*
		 * If we get here, the caller didn't provide enough
		 * storage to handle the response.  We need to still
		 * pull the data out of the socket, or we will get
		 * out of synce and be in trouble for the next
		 * call.
		 */
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
sock_read_leaf(int fd, char *path, int *len, char *data)
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
sock_write_leaf(int fd, char *path, int *len, char *data) {


	return(0);

}





#define	MAX_ENTS	128
void
dump_node(int fd, char *cur_path)
{

	dctl_entry_t	data[MAX_ENTS];
	int		ents;
	int		err, i;
	int		len;
	uint32_t	tmp;
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
		fprintf(stdout, "%s.%s ", cur_path, data[i].entry_name);
		sprintf(new_path, "%s.%s", cur_path, data[i].entry_name);
		len = sizeof(tmp);
		err = sock_read_leaf(fd, new_path, &len, (char *)&tmp);
		if (err) {
			exit(1);
		}
		fprintf(stdout, "= %d \n", tmp); 

	}
		
	ents = MAX_ENTS;
	err = sock_list_nodes(fd, cur_path, &ents, data);
	assert(err == 0);

	for (i = 0; i < ents; i++) {
		fprintf(stdout, "%s.%s.\n", cur_path, data[i].entry_name);
		sprintf(new_path, "%s.%s", cur_path, data[i].entry_name);
		dump_node(fd, new_path);
	}
}

void
dump_all(int fd)
{
	dctl_entry_t	data[MAX_ENTS];
	int		ents;
	int		err, i;

	ents = MAX_ENTS;
	err = sock_list_nodes(fd, "", &ents, data);
	assert(err == 0);

	for (i = 0; i < ents; i++) {
		char	new_path[256];
		fprintf(stdout, "%s.\n", data[i].entry_name);
		sprintf(new_path, "%s", data[i].entry_name);
		dump_node(fd, new_path);
	}

}



void
usage()
{


	fprintf(stdout, "dlog [-l level_flags] [-s source_flags] [-h] \n");

	fprintf(stdout, "\n");
	fprintf(stdout, "\t -l level_flags \n");
	fprintf(stdout, "\t\t sets the flags on which levels to log.  This \n");
	fprintf(stdout, "\t\t can be a hex value (with leading 0x) or it \n");
	fprintf(stdout, "\t\t be a comma (',') seperated list of symbolic \n");
	fprintf(stdout, "\t\t names,  The supported values are: \n\n");

	fprintf(stdout, "\n");
	fprintf(stdout, "\t -s source_flags \n");
	fprintf(stdout, "\t\t sets flags to indicate which data sources \n");
	fprintf(stdout, "\t\t should be included in the logging.  This can\n");
      	fprintf(stdout, "\t\t be a hex value (with leading 0x) or it \n");
	fprintf(stdout, "\t\t be a comma (',') seperated list of symbolic \n");
	fprintf(stdout, "\t\t names,  The supported values are: \n\n");


	fprintf(stdout, "\n");
	fprintf(stdout, "\t -h show this message \n");
}


int
main(int argc, char **argv)
{
	int	fd;
	struct sockaddr_un sa;
	int	err;
	int	c;

	/*
	 * The command line options.
	 */
	while (1) {
		c = getopt(argc, argv, "ah");
		if (c == -1) {
			break;
		}

		switch (c) {
			case 'a':
				/* dump_all(); */
				break;

			case 'h':
				usage();
				exit(0);
				break;


			default:
				printf("unknown option %c\n", c);
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
		fd = socket(PF_UNIX, SOCK_STREAM, 0);


		strcpy(sa.sun_path, SOCKET_DCTL_NAME);
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
			/*
			 * We sucessfully connection, now we set
			 * the parameters we want to log and
			 * read the log.
			 */
			dump_all(fd);
			sleep(1);
		}
		close(fd);
	}

	/* we should never get here, but it keeps gcc happy -:) */
	exit(0);
}


