/*
 * This provides the functions for accessing the rings
 */

#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include "ring.h"


ring_data_t *	my_ring;

void
simple_test()
{

	int 	i;
	int	err;
	void *	data;

	for (i = 1; i < RING_DATA_SIZE; i++) {
		err = ring_enq(my_ring, (void *)i);
		if (err) {
			printf("faild to write to ring on iteration %d \n", i);
			exit(1);
		}
	}

	/* make sure is empty return 0 */
	err = ring_empty(my_ring);
	if (err) {
		printf("non-empty ring returned wrong result \n");
		exit(1);
	}


	for (i = 1; i < RING_DATA_SIZE; i++) {
		data = ring_deq(my_ring);
		if (data == NULL) {
			printf("failed to return data on iteration %d \n", i);
			exit(1);
		}
		if (i != (int)data) {
			printf("wanted %d got %d \n", i, (int)data);
			exit(1);
		}
	}
	data = ring_deq(my_ring);
	if (data !=  NULL) {
		printf("deq returned data when it should be empty \n");
		exit(1);
	}

	/* make sure is empty return 0 */
	err = ring_empty(my_ring);
	if (!err) {
		printf("empty ring returned wrong result \n");
		exit(1);
	}
}


void
overflow_test()
{

	int 	i;
	int	err;
	void *	data;

	for (i = 1; i < (RING_DATA_SIZE); i++) {
		err = ring_enq(my_ring, (void *)i);
		if (err) {
			printf("failed to write to ring on iteration %d \n", i);
			exit(1);
		}
	}

	/* make sure is empty return 0 */
	err = ring_empty(my_ring);
	if (err) {
		printf("non-empty ring returned wrong result \n");
		exit(1);
	}

	/*
	 * We should overflow on this write */
	err = ring_enq(my_ring, (void *)0xdeaddead);
	if (!err) {
		printf("write to full worked \n");
	       	exit(1);

	}


	for (i = 1; i < (RING_DATA_SIZE); i++) {
		data = ring_deq(my_ring);
		if (data == NULL) {
			printf("failed to return data on iteration %d \n", i);
			exit(1);
		}
		if (i != (int)data) {
			printf("wanted %d got %d \n", i, (int)data);
			exit(1);
		}
	}
	data = ring_deq(my_ring);
	if (data !=  NULL) {
		printf("deq returned data when it should be empty \n");
		exit(1);
	}

	/* make sure is empty return 0 */
	err = ring_empty(my_ring);
	if (!err) {
		printf("empty ring returned wrong result \n");
		exit(1);
	}
}


int
main(int argc, char ** argv)
{

	int 	err;
	err = ring_init(&my_ring);
	if (err) {
		printf("failed to init ring \n");
		exit(1);
	}

	simple_test();
	simple_test();
	simple_test();

	overflow_test(); 
	overflow_test(); 
	overflow_test(); 
	overflow_test(); 

	return(0);
}



