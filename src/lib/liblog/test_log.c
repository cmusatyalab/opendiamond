#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include "lib_log.h"


#define	INNER_MAX	500
#define	OUTER_MAX	10000

void
empty_log()
{
	int	len;
	char *	data;


	while ((len = log_getbuf(&data)) != 0) {
		log_advbuf(len);
	}

}


void
test_many_writes()
{
	int		i, j, x;
	int		len;
	char *		data;
	int		cur_off;
	log_ent_t *	cur_ent;

	empty_log();

	/*
	 * Set the levels to where we want them to be.
	 */
	log_settype(LOGT_APP);
	log_setlevel(LOGL_INFO);

	for (i = 0; i < OUTER_MAX; i++) {
		for (j = 0; j < INNER_MAX; j++) {
			log_message(LOGT_APP, LOGL_INFO, "%d ", j);
		}

		/*
		 * Now read back the items and make sure they match
		 */
		j = 0;
		while ((len = log_getbuf(&data)) != 0) {
			cur_off = 0;
			while (cur_off < (len)) {
				cur_ent = (log_ent_t *)&data[cur_off];
				x = atoi(cur_ent->le_data);
				if (x != j) {
					printf("expected %d got %d \n", x, j);
					exit(1);
				}
				j++;
				cur_off += cur_ent->le_nextoff;
			}
			log_advbuf(len);
		}
		if (j != INNER_MAX) {
			printf("after all data: only retrieved %d \n", j);
			exit(1);
		}
	}
}



int
main(int argc, char **argv)
{

	log_init();
	test_many_writes();
	return(0);
}
