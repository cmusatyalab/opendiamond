/*
 * miniRPC - Simple TCP RPC library
 *
 * Copyright (C) 2007-2010 Carnegie Mellon University
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#include <stdint.h>
#include <unistd.h>
#include <semaphore.h>
#include <glib.h>
#include "common.h"

static sem_t cb_complete;
static gint ioerrs;

#define MINIRPC_PENDING -1

struct mrpc_header {
	uint32_t sequence;
	int32_t status;
	int32_t cmd;
	uint32_t datalen;
};

static void log_error(const gchar *domain, GLogLevelFlags level,
			const gchar *message, void *data)
{
	g_atomic_int_inc(&ioerrs);
}

static void send_msg(int fd, unsigned sequence, int status, int cmd,
			unsigned datalen)
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

static void _recv_msg(unsigned line, int fd, unsigned *sequence, int status,
			int cmd, unsigned datalen)
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

enum async_op {
	PING,
	RECV
};

struct async_data {
	struct mrpc_connection *conn;
	enum async_op op;
	mrpc_status_t expected;
};

static void *async_runner(void *data)
{
	struct async_data *ad = data;
	mrpc_status_t ret;

	switch (ad->op) {
	case PING:
		ret = proto_ping(ad->conn);
		break;
	case RECV:
		ret = recv_buffer_sync(ad->conn);
		break;
	default:
		g_assert_not_reached();
	}
	if (ad->expected != ret)
		die("async runner received status %d, expected %d", ret,
					ad->expected);
	sem_post(&cb_complete);
	g_slice_free(struct async_data, ad);
	return NULL;
}

static void async_call(struct mrpc_connection *conn, enum async_op op,
			mrpc_status_t expected)
{
	struct async_data *ad;
	pthread_t thr;
	pthread_attr_t attr;

	ad = g_slice_new0(struct async_data);
	ad->conn = conn;
	ad->op = op;
	ad->expected = expected;
	expect(pthread_attr_init(&attr), 0);
	expect(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED), 0);
	expect(pthread_create(&thr, &attr, async_runner, ad), 0);
	expect(pthread_attr_destroy(&attr), 0);
}

int main(int argc, char **argv)
{
	struct mrpc_connection *sconn;
	struct mrpc_connection *conn;
	int cfd;
	int sfd;
	int cfd_conn;
	int sfd_conn;
	unsigned seq;
	int expected_ioerrs=0;

	g_thread_init(NULL);
	g_log_set_handler("minirpc", G_LOG_LEVEL_MESSAGE, log_error, NULL);
	if (sem_init(&cb_complete, 0, 0))
		die("Couldn't initialize semaphore");

	get_conn_pair(&cfd, &sfd_conn);
	get_conn_pair(&cfd_conn, &sfd);
	expect(mrpc_conn_create(&sconn, proto_server, sfd_conn, NULL), 0);
	sync_server_set_ops(sconn);
	start_monitored_dispatcher(sconn);
	expect(mrpc_conn_create(&conn, proto_client, cfd_conn, NULL), 0);

	/* Successful ping on raw client */
	send_msg(cfd, 0, MINIRPC_PENDING, nr_proto_ping, 0);
	recv_msg(cfd, &seq, MINIRPC_OK, nr_proto_ping, 0);
	expect(seq, 0);

	/* Successful ping on raw server */
	async_call(conn, PING, 0);
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

	/* Call against procedure zero */
	send_msg(cfd, 6, MINIRPC_PENDING, 0, 0);
	recv_msg(cfd, &seq, MINIRPC_PROCEDURE_UNAVAIL, 0, 0);
	expect(seq, 6);

	/* Reply payload too short */
	async_call(conn, RECV, MINIRPC_ENCODING_ERR);
	recv_msg(sfd, &seq, MINIRPC_PENDING, nr_proto_recv_buffer, 0);
	send_msg(sfd, seq, MINIRPC_OK, nr_proto_recv_buffer, 500);
	sem_wait(&cb_complete);
	expected_ioerrs++;

	/* Reply payload too long */
	async_call(conn, RECV, MINIRPC_ENCODING_ERR);
	recv_msg(sfd, &seq, MINIRPC_PENDING, nr_proto_recv_buffer, 0);
	send_msg(sfd, seq, MINIRPC_OK, nr_proto_recv_buffer, 2000);
	sem_wait(&cb_complete);
	expected_ioerrs++;

	/* Zero-length reply payload when expecting nonzero */
	async_call(conn, RECV, MINIRPC_ENCODING_ERR);
	recv_msg(sfd, &seq, MINIRPC_PENDING, nr_proto_recv_buffer, 0);
	send_msg(sfd, seq, MINIRPC_OK, nr_proto_recv_buffer, 0);
	sem_wait(&cb_complete);
	expected_ioerrs++;

	/* Successful recv_buffer call (sanity check) */
	async_call(conn, RECV, MINIRPC_OK);
	recv_msg(sfd, &seq, MINIRPC_PENDING, nr_proto_recv_buffer, 0);
	send_msg(sfd, seq, MINIRPC_OK, nr_proto_recv_buffer, 1024);
	sem_wait(&cb_complete);

	/* Reply with both error code and payload */
	async_call(conn, RECV, MINIRPC_ENCODING_ERR);
	recv_msg(sfd, &seq, MINIRPC_PENDING, nr_proto_recv_buffer, 0);
	send_msg(sfd, seq, 25, nr_proto_recv_buffer, 1024);
	sem_wait(&cb_complete);
	expected_ioerrs++;

	/* Reply out of sequence */
	async_call(conn, PING, MINIRPC_ENCODING_ERR);
	recv_msg(sfd, &seq, MINIRPC_PENDING, nr_proto_ping, 0);
	send_msg(sfd, seq + 1, MINIRPC_OK, nr_proto_send_buffer, 0);
	sem_wait(&cb_complete);
	expected_ioerrs++;

	/* Reply with command not matching request command */
	async_call(conn, PING, MINIRPC_ENCODING_ERR);
	recv_msg(sfd, &seq, MINIRPC_PENDING, nr_proto_ping, 0);
	send_msg(sfd, seq, MINIRPC_OK, nr_proto_send_buffer, 0);
	sem_wait(&cb_complete);
	expected_ioerrs++;

	/* Unmatched reply */
	send_msg(cfd, 7, MINIRPC_OK, nr_proto_ping, 0);
	expected_ioerrs++;

	start_monitored_dispatcher(conn);

	/* Procedure call against NULL operations pointer */
	send_msg(sfd, 5, MINIRPC_PENDING, nr_proto_client_check_int, 4);
	recv_msg(sfd, &seq, MINIRPC_PROCEDURE_UNAVAIL,
				nr_proto_client_check_int, 0);
	expect(seq, 5);

	close(cfd);
	close(sfd);
	dispatcher_barrier();
	expect(expected_ioerrs == ioerrs, 1);
	sem_destroy(&cb_complete);
	return 0;
}
