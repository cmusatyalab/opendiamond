/* Example miniRPC client program */

#include <stdio.h>
#include <minirpc/minirpc.h>
#include "example_client.h"

#define die(s, args...) do {fprintf(stderr, s"\n", ##args); abort();} while (0)

int main(int argc, char **argv)
{
	char *host;
	char *port;
	struct mrpc_conn_set *set;
	struct mrpc_connection *conn;
	example_color_choice choice;
	example_count *count;
	int ret;

	if (argc != 3) {
		fprintf(stderr, "Usage: %s host port\n", argv[0]);
		exit(1);
	}
	host=argv[1];
	port=argv[2];

	if (mrpc_conn_set_create(&set, example_client, NULL))
		die("Couldn't create connection set");
	if (mrpc_start_dispatch_thread(set))
		die("Couldn't start dispatch thread");
	if (mrpc_conn_create(&conn, set, NULL))
		die("Couldn't create connection handle");
	ret=mrpc_connect(conn, AF_UNSPEC, host, port);
	if (ret)
		die("Couldn't connect to %s:%s: %s", host, port, strerror(ret));

	if (example_get_num_colors(conn, &count))
		die("get_num_colors failed");
	printf("Initial num_colors: %d\n", *count);
	free_example_count(count, 1);

	choice.acceptable.acceptable_val=malloc(4 * sizeof(example_color));
	choice.acceptable.acceptable_len=4;
	choice.acceptable.acceptable_val[0]=RED;
	choice.acceptable.acceptable_val[1]=ORANGE;
	choice.acceptable.acceptable_val[2]=GREEN;
	choice.acceptable.acceptable_val[3]=BLUE;
	choice.preferred=BLUE;
	if (example_choose_color(conn, &choice))
		die("choose_color failed");
	free_example_color_choice(&choice, 0);

	if (example_get_num_colors(conn, &count))
		die("get_num_colors failed");
	printf("Final num_colors: %d\n", *count);
	free_example_count(count, 1);

	mrpc_conn_close(conn);
	mrpc_conn_unref(conn);
	mrpc_conn_set_unref(set);
	return 0;
}
