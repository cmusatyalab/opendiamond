/* 
 * Program that slows the effective CPU speed by slowing down using
 * the fifo scheduling an sucking up cycles.
 */

#include <sys/io.h>		// ioperm(), iopl()
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <math.h>

#include <time.h>
#include <sched.h>
#include <errno.h>
#include <assert.h>

#define	MAX_DATA	1397

double	data[MAX_DATA];

void
compute(int num)
{
	int	index;

	index = num % MAX_DATA;

	data[index] = cos(num) * data[index];	
	data[index] = sin(data[index]) /(float)num;
			
}

void
usage()
{
	fprintf(stderr, "slowdown <percent decrease> \n");

}

#define	INIT_LOOP_VAL	100000
#define	INT_LOOP_CNT	20

double
tv_to_float(struct timeval *tv)
{
	double	val;

	val = (float)tv->tv_sec;
	val += ((float)((float)tv->tv_usec/1000000.0));   

	return(val);
}

int
over_adj_amt(double ratio, double target, int loop_val)
{
	int	new_target;
	int	delta;

	if ((ratio - target) > .10) {
		return(10000);
	} else if ((ratio - target) > .05) {
		return(1000);
	} else {
		return(100);
	}
}

int
under_adj_amt(double ratio, double target, int loop_val)
{
	int	new_target;
	int	delta;

	if ((target - ratio) > .1) {
		return(10000);
	} else if ((target - ratio) > .05) {
		return(1000);
	} else {
		return(100);
	}
}


int 
main(int argn, char* argv[]) 
{
	struct sched_param params;
	float	target;
	float	sum;
	unsigned int		loop_val = INIT_LOOP_VAL;;
	struct	rusage		user_time_start;
	struct	rusage		user_time_end;
	struct	timeval		wall_start;
	struct	timeval		wall_end;
	struct	timezone	tz;
	int		i,j, loop, err;
	double	wtime;
	double	app_time;
	double	ratio;



	target = atof(argv[1])/100.0;
	printf("target %f \n", target);
 
	sched_getparam(0, &params);
	params.sched_priority = 50;
	if (sched_setscheduler(0, SCHED_FIFO, &params)) {
		printf( "Error: %d\n", errno );
		exit(0);
	}
	


	for (loop = 0; loop < 1000000; loop++) {
		err = getrusage(RUSAGE_SELF, &user_time_start);
		assert(err == 0);
		err = gettimeofday(&wall_start, &tz);
		assert(err == 0);
		

		for (i=0; i < INT_LOOP_CNT; i++) {
			usleep(25000);

			for (j=1; j < loop_val; j++) {
				compute(loop_val);
			}
		}
		err = getrusage(RUSAGE_SELF, &user_time_end);
		assert(err == 0);
		err = gettimeofday(&wall_end, &tz);
		assert(err == 0);

		wtime = tv_to_float(&wall_end) - tv_to_float(&wall_start);

		app_time = tv_to_float(&user_time_end.ru_utime) +
			   tv_to_float(&user_time_end.ru_stime) -
			   tv_to_float(&user_time_start.ru_utime) - 
			   tv_to_float(&user_time_start.ru_stime);

		ratio = app_time/wtime;

		printf("ratio %f  target %f \n", ratio, target);

		if (ratio < target) {
			loop_val += under_adj_amt(ratio, target, loop_val);
		} else {
			loop_val -= over_adj_amt(ratio, target, loop_val);
		}

		printf("loop_val %d \n", loop_val);

	}
	return 0;
}

