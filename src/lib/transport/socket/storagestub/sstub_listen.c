/*
 * These file handles a lot of the device specific code.  For the current
 * version we have state for each of the devices.
 */
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/time.h>
#include <netdb.h>
#include <string.h>
#include <assert.h>
#include "ring.h"
#include "obj_attr.h"
#include "lib_odisk.h"
#include "lib_searchlet.h"
#include "socket_trans.h"
#include "lib_sstub.h"
#include "sstub_impl.h"





typedef enum {
	DEV_STOP,
	DEV_TERM,
	DEV_START,
	DEV_SEARCHLET
} dev_op_type_t;


typedef struct {
	char *filter;
	char *spec;
} dev_slet_data_t;


typedef struct {
	dev_op_type_t	cmd;
	int		id;
	union {
		dev_slet_data_t	sdata;
	} extra_data;

} dev_cmd_data_t;


/* set a socket to non-blocking */
static void
socket_non_block(int fd)
{
	int flags;

	flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1) {
		/* XXX */
	}
	fcntl(fd, F_SETFL, (flags | O_NONBLOCK));

}


/*
 * This is called if the connection has been shutdown the remote
 * side or some error connection.  We notify the caller of the library
 * and clean up the state.
 */

void
shutdown_connection(listener_state_t *lstate, cstate_t *cstate)
{

	/* set the flag to indicate we are shutting down */
	pthread_mutex_lock(&cstate->cmutex);
	cstate->flags |= CSTATE_SHUTTING_DOWN;
	pthread_mutex_unlock(&cstate->cmutex);


	/*
	 * Notify the "application" through the callback.
	 */
	(*lstate->close_conn_cb)(cstate->app_cookie);


	/* if there is a control socket, close it */
	if (cstate->flags & CSTATE_CNTRL_FD) {
		close(cstate->control_fd);
		cstate->flags &= ~CSTATE_CNTRL_FD;
	}

	/* if there is a data socket, close it */
	if (cstate->flags & CSTATE_DATA_FD) {
		close(cstate->data_fd);
		cstate->flags &= ~CSTATE_DATA_FD;
	}

	/* if there is a log socket, close it */
	if (cstate->flags & CSTATE_LOG_FD) {
		close(cstate->log_fd);
		cstate->flags &= ~CSTATE_LOG_FD;
	}

	/* clear the established flag */
	cstate->flags &= ~CSTATE_ESTABLISHED;


	/* XXX we need to clean up other state such as
	 * queued data ....
	 */
}

	
/*
 * Create and establish a socket with the other
 * side.
 */
int
sstub_new_sock(int *fd, int port)
{
	int 			new_fd;
	struct protoent	*	pent;
	struct sockaddr_in	sa;
	int			err;
	int			yes = 1;

	pent = getprotobyname("tcp");
	if (pent == NULL) {
		/* XXX log error */
		return(ENOENT);
	}

	new_fd = socket(PF_INET, SOCK_STREAM, pent->p_proto);
	/* XXX err ?? */


	/*
	 * set the socket options to avoid the re-use message if
	 * we have a fast restart.
	 */

	err = setsockopt(new_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
	if (err == -1) {
		/* XXX log */
		perror("setsockopt");
		return(ENOENT);
       }


	sa.sin_family = AF_INET;
	sa.sin_port = htons((unsigned short) port);
	sa.sin_addr.s_addr = htonl(INADDR_ANY);

	err = bind(new_fd, (struct sockaddr *)&sa, sizeof(sa));
	if (err) {
		/* XXX log */
		perror("bind failed ");
		return(ENOENT);
	}

	err = listen(new_fd, SOMAXCONN);
	if (err) {
		/* XXX log */
		printf("bind failed \n");
		return(ENOENT);
	}

	*fd = new_fd;
	return(0);
}


/*
 * We have all the sockets endpoints open for this new connetion.
 * Now we span a thread for managing this connection as well
 * using the callbacks to inform the caller that there is a new
 * connection to be serviced.
 */
static void
have_full_conn(listener_state_t *list_state, int conn)
{

	int 			err;
	void * 			new_cookie;
	cstate_t *		cstate;
	pid_t			new_proc;


	cstate = &list_state->conns[conn];
	err = ring_2init(&cstate->obj_ring);
	if (err) {
		/* XXX */
		printf("failed to init obj ring \n");
		return;	
	}

	err = ring_init(&cstate->control_tx_ring);
	if (err) {
		/* XXX */
		printf("failed to init control ring \n");
		return;	
	}

	/*
	 * Now we fork a new process.  The child will invoke the callback
	 * and handle the new connections.  The parent will clean up
	 * the local state.
	 */

	new_proc = fork();

	if (new_proc == 0) {
		printf("XXX child process \n");

		err = (*list_state->new_conn_cb)((void *)cstate, &new_cookie);
		if (err) {
			printf("new conn callback failed \n");
			/* XXX clean up sockets */
			return;
		}

		/*
	 	 * we registered correctly, save the cookie and create a thread
	  	 */

		list_state->conns[conn].app_cookie = new_cookie;

		/*
		 * the main thread for this process is used
		 * for servicing the connections.
		 */
		connection_main(list_state, conn);		
		return;
	} else {
		printf("parent !!! \n");
		shutdown_connection(list_state, cstate);
	}
}


/*
 * This accepts an incomming connection request to the control
 * port.  We accept the connection and assign it to a new connection
 * state.
 */

static void
accept_control_conn(listener_state_t *list_state)
{
	struct sockaddr_in	ca;
	int			csize;
	int			new_sock;
	int			i;
	uint32_t		data;
	size_t			wsize;
	printf("accepting control !!! \n");

	csize = sizeof(ca);
	new_sock = accept(list_state->control_fd, (struct sockaddr *)
			&ca, &csize);

	if (new_sock < 0) {
		/* XXX log */
		printf("XXX accept failed \n");	
	}

	printf("new cntrl conn from %s \n", inet_ntoa(ca.sin_addr));

	/*
	 * Now we allocate a per connection state information and
	 * store the socket associated with this.
	 */
	for (i = 0; i < MAX_CONNS; i++) {
		if (!(list_state->conns[i].flags & CSTATE_ALLOCATED)) {
			break;
		}
	}
	if (i == MAX_CONNS) {
		/* XXX log */
		printf("XXX accept control no state \n");
		close(new_sock);
		return;
	}

	list_state->conns[i].flags |= CSTATE_ALLOCATED;
	list_state->conns[i].flags |= CSTATE_CNTRL_FD;
	list_state->conns[i].control_fd = new_sock;
	/* XXX return */
	pthread_mutex_init(&list_state->conns[i].cmutex, NULL);

	data = (uint32_t)i;

	wsize = send(new_sock, (char *)&data, sizeof(data), 0);
	if (wsize < 0) {
		/* XXX log */
		printf("XXX Failed write on cntrl connection \n");
		close(new_sock);
		list_state->conns[i].flags &= ~CSTATE_ALLOCATED;
		return;
	}
	socket_non_block(new_sock);

}

/*
 * This accepts an incomming connection request to the data
 * port.  We accept the connection and get some data out of it.  This
 * should tell us what connection it belongs to (if the caller did
 * the correct handshake.  
 */

static void
accept_data_conn(listener_state_t *list_state)
{
	struct sockaddr_in	ca;
	int			csize;
	int			new_sock;
	uint32_t		data;
	size_t			dsize;


	printf("accepting data !!! \n");

	csize = sizeof(ca);
	new_sock = accept(list_state->data_fd, (struct sockaddr *)
			&ca, &csize);

	if (new_sock < 0) {
		/* XXX log */
		printf("XXX accept failed \n");	
	}

	dsize = recv(new_sock, (char *)&data, sizeof(data), 0);
	if (dsize < 0) {
		/* XXX */
		printf("failed read cookie \n");
		close(new_sock);
		return;
	}
	printf("after data read !! \n");

	if (data >= MAX_CONNS) {
		/* XXX */
		printf("data conn cookie out of range \n");
		close(new_sock);
		return;
	}


	if (!(list_state->conns[data].flags & CSTATE_ALLOCATED)) {
		/* XXX */
		printf("connection not on valid cookie \n");
		close(new_sock);
		return;
	}


	printf("new data conn from %s \n", inet_ntoa(ca.sin_addr));

	list_state->conns[data].flags |= CSTATE_DATA_FD;
	list_state->conns[data].data_fd = new_sock;

	if ((list_state->conns[data].flags & CSTATE_ALL_FD) ==
		       CSTATE_ALL_FD)	{
		have_full_conn(list_state, (int) data);
			
	}

	socket_non_block(new_sock);
	
}


static void
accept_log_conn(listener_state_t *list_state)
{
	struct sockaddr_in	ca;
	int			csize;
	int			new_sock;
	uint32_t		data;
	size_t			dsize;


	printf("accepting log !!! \n");

	csize = sizeof(ca);
	new_sock = accept(list_state->log_fd, (struct sockaddr *)
			&ca, &csize);

	if (new_sock < 0) {
		/* XXX log */
		printf("XXX accept failed \n");	
	}

	dsize = recv(new_sock, (char *)&data, sizeof(data), 0);
	if (dsize < 0) {
		/* XXX */
		printf("failed read cookie \n");
		close(new_sock);
		return;
	}

	if (data >= MAX_CONNS) { 
		/* XXX */
		printf("data conn cookie out of range \n");
		close(new_sock);
		return;
	}	

	if (!(list_state->conns[data].flags & CSTATE_ALLOCATED)) {
		/* XXX */
		printf("connection not on valid cookie \n");
		close(new_sock);
		return;
	}

	printf("new log conn from %s \n", inet_ntoa(ca.sin_addr));

	list_state->conns[data].flags |= CSTATE_LOG_FD;
	list_state->conns[data].log_fd = new_sock;

	if ((list_state->conns[data].flags & CSTATE_ALL_FD) ==
		       CSTATE_ALL_FD)	{
		have_full_conn(list_state, (int) data);
	}
	socket_non_block(new_sock);
}


/*
 * This is the main thread for the listner.  This listens for
 * new incoming connection requests and creats the new connection
 * information associated with them.
 */

void 
sstub_listen(void *cookie)
{
	listener_state_t *list_state;
	struct timeval now;
	int	err;
	int	max_fd = 0; 

	list_state = (listener_state_t *)cookie;


	max_fd = list_state->control_fd;
	if (list_state->data_fd > max_fd) {
		max_fd = list_state->data_fd;
	}
	if (list_state->log_fd > max_fd) {
		max_fd = list_state->log_fd;
	}
	max_fd += 1;


	
	while (1) {
		FD_ZERO(&list_state->read_fds);
		FD_ZERO(&list_state->write_fds);
		FD_ZERO(&list_state->except_fds);

		FD_SET(list_state->control_fd,  &list_state->read_fds);
		FD_SET(list_state->data_fd,  &list_state->read_fds);
		FD_SET(list_state->log_fd,  &list_state->read_fds);


		now.tv_sec = 1;
		now.tv_usec = 0;

		/*
		 * Sleep on the set of sockets to see if anything
		 * interesting has happened.
		 */
		err = select(max_fd, &list_state->read_fds, 
				&list_state->write_fds, 
				&list_state->except_fds,  &now);
		if (err == -1) {
			/* XXX log */
			printf("XXX select failed \n");
			exit(1);
		}

		/* 
		 * If err > 0 then there are some objects
		 * that have data.
		 */
		if (err > 0) {
			if (FD_ISSET(list_state->control_fd, 
				    &list_state->read_fds)) {
				accept_control_conn(list_state);
			}

			if (FD_ISSET(list_state->data_fd, 
				    &list_state->read_fds)) {
				accept_data_conn(list_state);			
			}
			if (FD_ISSET(list_state->log_fd, 
				    &list_state->read_fds)) {
				accept_log_conn(list_state);			
			}
		}
	}
}






