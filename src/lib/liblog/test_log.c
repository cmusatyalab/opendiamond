/*
 * 	Diamond (Release 1.0)
 *      A system for interactive brute-force search
 *
 *      Copyright (c) 2002-2005, Intel Corporation
 *      All Rights Reserved
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <netinet/in.h>
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
string_test()
{
	int		i;
	int		len;
	char * 		data;
	int		cur_off;
	log_ent_t *	cur_ent;

	empty_log();

	/*
	 * Set the levels to where we want them to be.
	 */
	log_settype(LOGT_APP);
	log_setlevel(LOGL_INFO);

	/*
	 * Write a bunch of strings of different lengths and contents
	 */
	for (i = 0; i < 1; i++) {
		log_message(LOGT_APP, LOGL_INFO, "A\n");
		log_message(LOGT_APP, LOGL_INFO, "BB\n");
		log_message(LOGT_APP, LOGL_INFO, "CCC\n");
		log_message(LOGT_APP, LOGL_INFO, "DDD\n");
		log_message(LOGT_APP, LOGL_INFO, "EEEE\n");
		log_message(LOGT_APP, LOGL_INFO, "%s %s ", "sldkfj", "abc");
	}


	len = log_getbuf(&data);

	cur_off = 0;
	while (cur_off < (len)) {
		cur_ent = (log_ent_t *)&data[cur_off];
		printf("str_test: <%s> \n", cur_ent->le_data);
		cur_off += ntohl(cur_ent->le_nextoff);
	}
	log_advbuf(len);
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
				cur_off += ntohl(cur_ent->le_nextoff);
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
	void	*foo;

	log_init(&foo);
	string_test();
	test_many_writes();
	return(0);
}
