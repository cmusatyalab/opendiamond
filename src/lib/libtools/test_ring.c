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
ring_data_t *	my_2ring;

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

void
overflow_2test()
{

	int 	i;
	int	err;
	void *	d1;
	void *	d2;

	/* First fill up the ring */
	for (i = 1; i < (RING_DATA_SIZE -1); i+= 2) {
		err = ring_2enq(my_2ring, (void *)i, (void *)i+1);
		if (err) {
			printf("overflow_2test: failed write on %d \n", i);
			exit(1);
		}
	}

	/* make sure is empty return 0 */
	err = ring_2empty(my_2ring);
	if (err) {
		printf("overflow_2test: ring_empty returned wrong result \n");
		exit(1);
	}

	/*
	 * We should overflow on this write */
	err = ring_2enq(my_2ring, (void *)0xdeaddead, (void *)0xdeadbeef);
	if (!err) {
		printf("overflow_2test: write to full worked \n");
	       	exit(1);
	}


	/* pull all the data off the ring */
	for (i = 1; i < (RING_DATA_SIZE - 1); i+=2) {
		err = ring_2deq(my_2ring, &d1, &d2);
		if (err != 0) {
			printf("failed to return data on iteration %d \n", i);
			exit(1);
		}
		if ((i != (int)d1) || ((i+1) != (int)d2)) {
			printf("wanted %d, %d got %d, %d \n", i, i+1,
				       	(int)d1, (int)d2);
			exit(1);
		}
	}


	/* try to get other data off the ring, this should fail */
	err = ring_2deq(my_2ring, &d1, &d2);
	if (err ==  0) {
		printf("overflow_2test: deq to empty ring returned data\n");
		exit(1);
	}

	/* make sure is empty return 0 */
	err = ring_2empty(my_2ring);
	if (!err) {
		printf("empty ring returned wrong result \n");
		exit(1);
	}
}

void
simple_2test()
{

	int 	i;
	int	err;
	void *	data1;
	void *	data2;

	for (i = 1; i < (RING_DATA_SIZE - 1); i+= 2) {
		err = ring_2enq(my_2ring, (void *)i, (void *)i+1);
		if (err) {
			printf("simple_2test: failed to write on iter %d \n", 
					i);
			exit(1);
		}
	}

	/* make sure is empty return 0 */
	err = ring_2empty(my_2ring);
	if (err) {
		printf("simple_2test: non-empty ring returned wrong result \n");
		exit(1);
	}


	for (i = 1; i < (RING_DATA_SIZE -1); i+= 2) {
		err = ring_2deq(my_2ring, &data1, &data2);
		if (err != 0) {
			printf("simple_2test: failed return on %d \n", i);
			exit(1);
		}
		if (((int)data1 != i) || ((int)data2 != (i+1))) {
			printf("wanted %d %d got %d %d \n", i, i+1, 
					(int)data1, (int)data2);
			exit(1);
		}
	}
	err = ring_2deq(my_2ring, &data1, &data2);
	if (err == 0) {
		printf("simple_2test: deq returned data on empty queue \n");
		exit(1);
	}

	/* make sure is_ empty return 0 */
	err = ring_2empty(my_2ring);
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


	err = ring_2init(&my_2ring);
	if (err) {
		printf("failed to init double ring \n");
		exit(1);
	}

	simple_2test();
	simple_2test();
	simple_2test();
	simple_2test();

	overflow_2test(); 
	overflow_2test(); 
	overflow_2test(); 
	overflow_2test(); 
	overflow_2test(); 

	return(0);
}



