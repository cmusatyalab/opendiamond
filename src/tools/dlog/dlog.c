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
#include <lib_log.h>
#include "log.h"

#define	MAX_BUFFER	4096


#define	MAX_LEVEL	8
#define	MAX_TYPE	8


/*
 * Convert the level to a human readable string.
 */

void
get_level_string(uint32_t level, char *string, int max)
{


	switch(level) {

		case LOGL_CRIT:
			strncpy(string, "Crit", max);
			string[max-1] = '\0';
			break;

		case LOGL_ERR:
			strncpy(string, "Err", max);
			string[max-1] = '\0';
			break;

		case LOGL_INFO:
			strncpy(string, "Info", max);
			string[max-1] = '\0';
			break;

		case LOGL_TRACE:
			strncpy(string, "Trace", max);
			string[max-1] = '\0';
			break;

		default:
			strncpy(string, "Unknown", max);
			string[max-1] = '\0';
			break;
	}
}

void
get_type_string(uint32_t level, char *string, int max)
{


	switch(level) {

		case LOGT_APP:
			strncpy(string, "App", max);
			string[max-1] = '\0';
			break;

		case LOGT_DISK:
			strncpy(string, "Disk", max);
			string[max-1] = '\0';
			break;

		case LOGT_FILT:
			strncpy(string, "Filt", max);
			string[max-1] = '\0';
			break;

		case LOGT_BG:
			strncpy(string, "Background", max);
			string[max-1] = '\0';
			break;

		default:
			strncpy(string, "Unknown", max);
			string[max-1] = '\0';
			break;
	}
}


void
display_results(log_msg_t *lheader, char *data)
{
	char		source;
	char *		host_id;
	struct in_addr 	iaddr;
	log_ent_t *	log_ent;
	uint32_t	level;
	uint32_t	type;
	uint32_t	dlen;
	char		level_string[MAX_LEVEL];
	char		type_string[MAX_TYPE];
	int		cur_offset, total_len;



	/*
	 * Setup the source of the data.
	 */
	if (lheader->log_type == LOG_SOURCE_BACKGROUND) {
		source = 'H';
	} else {
		source = 'D';
	}


	iaddr.s_addr = lheader->dev_id;

	host_id = inet_ntoa(iaddr);

	total_len = lheader->log_len;
	cur_offset = 0;

	while (cur_offset < total_len) {
		log_ent = (log_ent_t *)&data[cur_offset];


		level = ntohl(log_ent->le_level);	
		type = ntohl(log_ent->le_type);	
		dlen = ntohl(log_ent->le_dlen);
		log_ent->le_data[dlen - 1] = '\0';

		get_level_string(level, level_string, MAX_LEVEL);
		get_type_string(type, type_string, MAX_LEVEL);

		fprintf(stdout, "<%c %s %s %s> %s \n",
			       	source, host_id, level_string, type_string, 
				log_ent->le_data);

		cur_offset += ntohl(log_ent->le_nextoff);

	}




}

void
read_log(int fd)
{
	int	len;
	log_msg_t	lheader;
	char 	*data;

	while (1) {
		len = recv(fd, &lheader, sizeof(lheader), MSG_WAITALL);
		if (len != sizeof(lheader)) {
			return;
		}

		data = (char *)malloc(lheader.log_len);	
		if (data == NULL) {
			perror("Failed malloc \n");
			exit(1);
		}

		len = recv(fd, data, lheader.log_len, MSG_WAITALL);
		if (len != lheader.log_len) {
			return;
		}

		display_results(&lheader, data);

		free(data);
	}


}


int
main(int argc, char **argv)
{
	int	fd;
	struct sockaddr_un sa;
	int	err;



	while (1) {
		fd = socket(PF_UNIX, SOCK_STREAM, 0);


		strcpy(sa.sun_path, SOCKET_LOG_NAME);
		sa.sun_family = AF_UNIX;

	
		err = connect(fd, (struct sockaddr *)&sa, sizeof (sa));
		if (err < 0) {
			sleep(1);
		} else {
			read_log(fd);
		}
		close(fd);
	}
}



