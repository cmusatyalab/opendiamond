/*
 * miniRPC - TCP RPC library with asynchronous operations
 *
 * Copyright (C) 2007-2008 Carnegie Mellon University
 *
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU Lesser General Public License version 2.1, as shown in
 * the file COPYING.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdint.h>
#include <unistd.h>
#include <semaphore.h>
#include <glib.h>
#include "common.h"

static sem_t cb_complete;

struct mrpc_header {
	uint32_t sequence;
	int32_t status;
	int32_t cmd;
	uint32_t datalen;
};

void send_msg(int fd, unsigned sequence, int status, int cmd, unsigned datalen)
{
	struct mrpc_header hdr;
	char *payload;

	hdr.sequence=htonl(sequence);
	hdr.status=htonl(status);
	hdr.cmd=htonl(cmd);
	hdr.datalen=htonl(datalen);
	if (write(fd, &hdr, sizeof(hdr)) != sizeof(hdr))
		die("Short write on header");
	if (datalen) {
		payload=malloc(datalen);
		memset(payload, 0, datalen);
		if (write(fd, payload, datalen) != (ssize_t) datalen)
			die("Short write on data");
		free(payload);
	}
}

void _recv_msg(unsigned line, int fd, unsigned *sequence, int status, int cmd,
			unsigned datalen)
{
	struct mrpc_header hdr;
	char *payload;

	if (read(fd, &hdr, sizeof(hdr)) != sizeof(hdr))
		die("Short read on header");
	hdr.sequence=ntohl(hdr.sequence);
	hdr.status=ntohl(hdr.status);
	hdr.cmd=ntohl(hdr.cmd);
	hdr.datalen=ntohl(hdr.datalen);
	*sequence=hdr.sequence;
	if (hdr.status != status)
		die("Read (line %d): expected status %d, found %d", line,
					status, hdr.status);
	if (hdr.cmd != cmd)
		die("Read (line %d): expected cmd %d, found %d", line, cmd,
					hdr.cmd);
	if (hdr.datalen != datalen)
		die("Read (line %d): expected datalen %u, found %u", line,
					datalen, hdr.datalen);
	if (hdr.datalen) {
		payload=malloc(datalen);
		if (read(fd, payload, datalen) != (ssize_t) datalen)
			die("Short read on data");
		free(payload);
	}
}
#define recv_msg(args...) _recv_msg(__LINE__, args)

void cb_func(void *msg_private, mrpc_status_t status)
{
	mrpc_status_t *expected=msg_private;
	if (status != *expected)
		die("cb_func received status %d, expected %d", status,
					*expected);
	sem_post(&cb_complete);
}

void cb_ping(void *conn_private, void *msg_private, mrpc_status_t status)
{
	cb_func(msg_private, status);
}

void cb_recv(void *conn_private, void *msg_private, mrpc_status_t status,
			KBuffer *out)
{
	cb_func(msg_private, status);
}

int main(int argc, char **argv)
{
	struct mrpc_conn_set *sset;
	struct mrpc_conn_set *cset;
	struct mrpc_connection *conn;
	struct sockaddr_in addr;
	socklen_t addrlen;
	char *set_port;
	char skt_port[8];
	int cfd;
	int lfd;
	int sfd;
	unsigned seq;
	mrpc_status_t expected;
	int expected_ioerrs=0;

	if (sem_init(&cb_complete, 0, 0))
		die("Couldn't initialize semaphore");
	sset=spawn_server(&set_port, proto_server, sync_server_accept, NULL,
				1);
	mrpc_set_disconnect_func(sset, disconnect_normal);
	mrpc_set_ioerr_func(sset, handle_ioerr);
	if (mrpc_conn_set_create(&cset, proto_client, NULL))
		die("Couldn't create client conn set");
	mrpc_set_disconnect_func(cset, disconnect_normal);
	mrpc_set_ioerr_func(cset, handle_ioerr);
	start_monitored_dispatcher(cset);

	cfd=socket(PF_INET, SOCK_STREAM, 0);
	if (cfd == -1)
		die("Couldn't create socket");
	addr.sin_family=AF_INET;
	addr.sin_port=htons(atoi(set_port));
	addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
	if (connect(cfd, (struct sockaddr *)&addr, sizeof(addr)))
		die("Couldn't connect socket");
	free(set_port);
	lfd=socket(PF_INET, SOCK_STREAM, 0);
	if (lfd == -1)
		die("Couldn't create socket");
	addr.sin_port=0;
	if (bind(lfd, (struct sockaddr *)&addr, sizeof(addr)))
		die("Couldn't bind socket");
	if (listen(lfd, 16))
		die("Couldn't listen on socket");
	addrlen=sizeof(addr);
	if (getsockname(lfd, (struct sockaddr *)&addr, &addrlen))
		die("Couldn't get socket name");
	snprintf(skt_port, sizeof(skt_port), "%u", ntohs(addr.sin_port));
	if (mrpc_conn_create(&conn, cset, NULL))
		die("Couldn't create conn handle");
	if (mrpc_connect(conn, AF_INET, NULL, skt_port))
		die("Couldn't connect to listening socket");
	sfd=accept(lfd, NULL, NULL);
	if (sfd == -1)
		die("Couldn't accept incoming connection");
	close(lfd);

	/* Successful ping on raw client */
	send_msg(cfd, 0, MINIRPC_PENDING, nr_proto_ping, 0);
	recv_msg(cfd, &seq, MINIRPC_OK, nr_proto_ping, 0);
	expect(seq, 0);

	/* Successful ping on raw server */
	expected=0;
	expect(proto_ping_async(conn, cb_ping, &expected), 0);
	recv_msg(sfd, &seq, MINIRPC_PENDING, nr_proto_ping, 0);
	send_msg(sfd, seq, MINIRPC_OK, nr_proto_ping, 0);
	sem_wait(&cb_complete);

	/* Request payload too short */
	send_msg(cfd, 1, MINIRPC_PENDING, nr_proto_send_buffer, 500);
	recv_msg(cfd, &seq, MINIRPC_ENCODING_ERR, nr_proto_send_buffer, 0);
	expect(seq, 1);
	expected_ioerrs++;

	/* Request payload too long */
	send_msg(cfd, 2, MINIRPC_PENDING, nr_proto_send_buffer, 2000);
	recv_msg(cfd, &seq, MINIRPC_ENCODING_ERR, nr_proto_send_buffer, 0);
	expect(seq, 2);
	expected_ioerrs++;

	/* Zero-length request payload when expecting non-zero */
	send_msg(cfd, 3, MINIRPC_PENDING, nr_proto_send_buffer, 0);
	recv_msg(cfd, &seq, MINIRPC_ENCODING_ERR, nr_proto_send_buffer, 0);
	expect(seq, 3);
	expected_ioerrs++;

	/* Bogus procedure */
	send_msg(cfd, 4, MINIRPC_PENDING, 12345, 500);
	recv_msg(cfd, &seq, MINIRPC_PROCEDURE_UNAVAIL, 12345, 0);
	expect(seq, 4);

	/* Procedure call against NULL operations pointer */
	send_msg(sfd, 5, MINIRPC_PENDING, nr_proto_client_check_int, 4);
	recv_msg(sfd, &seq, MINIRPC_PROCEDURE_UNAVAIL,
				nr_proto_client_check_int, 0);
	expect(seq, 5);

	/* Call against procedure zero */
	send_msg(cfd, 6, MINIRPC_PENDING, 0, 0);
	recv_msg(cfd, &seq, MINIRPC_PROCEDURE_UNAVAIL, 0, 0);
	expect(seq, 6);

	/* Reply payload too short */
	expected=MINIRPC_ENCODING_ERR;
	expect(proto_recv_buffer_async(conn, cb_recv, &expected), 0);
	recv_msg(sfd, &seq, MINIRPC_PENDING, nr_proto_recv_buffer, 0);
	send_msg(sfd, seq, MINIRPC_OK, nr_proto_recv_buffer, 500);
	sem_wait(&cb_complete);
	expected_ioerrs++;

	/* Reply payload too long */
	expected=MINIRPC_ENCODING_ERR;
	expect(proto_recv_buffer_async(conn, cb_recv, &expected), 0);
	recv_msg(sfd, &seq, MINIRPC_PENDING, nr_proto_recv_buffer, 0);
	send_msg(sfd, seq, MINIRPC_OK, nr_proto_recv_buffer, 2000);
	sem_wait(&cb_complete);
	expected_ioerrs++;

	/* Zero-length reply payload when expecting nonzero */
	expected=MINIRPC_ENCODING_ERR;
	expect(proto_recv_buffer_async(conn, cb_recv, &expected), 0);
	recv_msg(sfd, &seq, MINIRPC_PENDING, nr_proto_recv_buffer, 0);
	send_msg(sfd, seq, MINIRPC_OK, nr_proto_recv_buffer, 0);
	sem_wait(&cb_complete);
	expected_ioerrs++;

	/* Successful recv_buffer call (sanity check) */
	expected=MINIRPC_OK;
	expect(proto_recv_buffer_async(conn, cb_recv, &expected), 0);
	recv_msg(sfd, &seq, MINIRPC_PENDING, nr_proto_recv_buffer, 0);
	send_msg(sfd, seq, MINIRPC_OK, nr_proto_recv_buffer, 1024);
	sem_wait(&cb_complete);

	/* Reply with both error code and payload */
	expected=MINIRPC_ENCODING_ERR;
	expect(proto_recv_buffer_async(conn, cb_recv, &expected), 0);
	recv_msg(sfd, &seq, MINIRPC_PENDING, nr_proto_recv_buffer, 0);
	send_msg(sfd, seq, 25, nr_proto_recv_buffer, 1024);
	sem_wait(&cb_complete);
	expected_ioerrs++;

	/* Reply with command not matching request command */
	expected=MINIRPC_ENCODING_ERR;
	expect(proto_ping_async(conn, cb_ping, &expected), 0);
	recv_msg(sfd, &seq, MINIRPC_PENDING, nr_proto_ping, 0);
	send_msg(sfd, seq, MINIRPC_OK, nr_proto_send_buffer, 0);
	sem_wait(&cb_complete);
	expected_ioerrs++;

	/* Unmatched reply */
	send_msg(cfd, 7, MINIRPC_OK, nr_proto_ping, 0);
	expected_ioerrs++;

	/* Unidirectional message with error code */
	send_msg(cfd, 8, MINIRPC_OK, nr_proto_msg_buffer, 1024);
	expected_ioerrs++;

	/* Empty event queues before closing, to make sure that no ioerr
	   events are squashed */
	send_msg(cfd, 0, MINIRPC_PENDING, nr_proto_ping, 0);
	recv_msg(cfd, &seq, MINIRPC_OK, nr_proto_ping, 0);
	expect(seq, 0);
	expected=0;
	expect(proto_ping_async(conn, cb_ping, &expected), 0);
	recv_msg(sfd, &seq, MINIRPC_PENDING, nr_proto_ping, 0);
	send_msg(sfd, seq, MINIRPC_OK, nr_proto_ping, 0);
	sem_wait(&cb_complete);

	close(cfd);
	close(sfd);
	mrpc_listen_close(sset);
	mrpc_conn_set_unref(sset);
	mrpc_conn_set_unref(cset);
	expect_disconnects(0, 2, 0);
	expect_ioerrs(expected_ioerrs);
	sem_destroy(&cb_complete);
	return 0;
}
