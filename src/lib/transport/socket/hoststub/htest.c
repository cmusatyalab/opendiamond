#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/time.h>
#include <netdb.h>
#include "ring.h"
#include "obj_attr.h"
#include "lib_odisk.h"
#include "lib_searchlet.h"
#include "socket_trans.h"
#include "lib_hstub.h"


int
handle_new_obj(void *hcookie, obj_data_t *odata, int vno)
{

	printf("new_obj!! - vno  %d \n", vno);


}


int
main(int argc, char **argv)
{
	int	err;
	void *	cookie;

	cookie = device_init(100, "127.0.0.1", 0, handle_new_obj);


	err = device_start(cookie, 101);
	if (err) {
		printf("failed to start device \n");
	}

	err = device_set_searchlet(cookie, 102, "/tmp/x", "/tmp/y" );

	printf("XXX sending stop \n");
	err = device_stop(cookie, 102);
	if (err) {
		printf("failed to stop device \n");
	}

	printf("sleeping \n");
		sleep(10);
	printf("done sleep \n");

}
