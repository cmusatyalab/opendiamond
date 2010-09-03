/* Example miniRPC server program */

#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <minirpc/minirpc.h>
#include "example_server.h"

#define die(s, args...) do {fprintf(stderr, s"\n", ##args); abort();} while (0)

struct conn_data {
	struct mrpc_connection *conn;
	char *remote;
	unsigned chosen;
	int count;
};

mrpc_status_t do_choose(void *conn_data, struct mrpc_message *msg,
			example_color_choice *in)
{
	struct conn_data *data=conn_data;
	unsigned i;
	unsigned new=0;

	/* Save a bitmap of the colors chosen by the client.  For this
	   example, we don't actually do anything with the bitmap. */
	for (i=0; i<in->acceptable.acceptable_len; i++)
		new |= 1 << in->acceptable.acceptable_val[i];
	data->chosen=new;
	data->count=i;
	return MINIRPC_OK;
}

mrpc_status_t do_get(void *conn_data, struct mrpc_message *msg,
			example_count *out)
{
	struct conn_data *data=conn_data;

	*out=data->count;
	return MINIRPC_OK;
}

static const struct example_server_operations ops = {
	.choose_color = do_choose,
	.get_num_colors = do_get,
	.crayon_selected = NULL,  /* not supported */
};

void *do_accept(void *set_data, struct mrpc_connection *conn,
			struct sockaddr *from, socklen_t from_len)
{
	struct conn_data *data;
	char buf[256];

	if (getnameinfo(from, from_len, buf, sizeof(buf), NULL, 0, 0)) {
		mrpc_conn_close(conn);
		return NULL;
	}
	example_server_set_operations(conn, &ops);
	data=malloc(sizeof(*data));
	memset(data, 0, sizeof(*data));
	data->conn=conn;
	data->remote=strndup(buf, sizeof(buf));
	fprintf(stderr, "New connection from %s\n", data->remote);
	return data;
}

void do_disconnect(void *conn_data, enum mrpc_disc_reason reason)
{
	struct conn_data *data=conn_data;
	const char *reason_str;

	/* If do_accept() rejected the connection and returned NULL, we have
	   nothing to free. */
	if (conn_data == NULL)
		return;

	switch (reason) {
	case MRPC_DISC_USER:
		reason_str="by application";
		break;
	case MRPC_DISC_CLOSED:
		reason_str="by remote end";
		break;
	case MRPC_DISC_IOERR:
		reason_str="due to I/O error";
		break;
	default:
		reason_str="for unknown reason";
	}

	fprintf(stderr, "Connection to %s closed %s\n", data->remote,
				reason_str);
	free(data->remote);
	free(data);
}

int main(int argc, char **argv)
{
	struct mrpc_conn_set *set;
	char *port;
	int ret;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s port\n", argv[0]);
		exit(1);
	}
	port=argv[1];

	if (mrpc_conn_set_create(&set, example_server, NULL))
		die("Couldn't create connection set");
	if (mrpc_set_accept_func(set, do_accept))
		die("Couldn't set accept function");
	if (mrpc_set_disconnect_func(set, do_disconnect))
		die("Couldn't set disconnect function");
	ret=mrpc_listen(set, AF_UNSPEC, NULL, &port);
	if (ret)
		die("Couldn't create listening socket: %s", strerror(ret));

	mrpc_dispatcher_add(set);
	mrpc_dispatch_loop(set);
	/* For this simple example, we don't ever destroy the connection set,
	   so mrpc_dispatch_loop() will never return. */
	return 0;
}
