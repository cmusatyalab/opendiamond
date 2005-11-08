/*
 *      Diamond (Release 1.0)
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


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "rstat.h"


static char const cvsid[] =
    "$Header$";

/*
 * return NULL if not found 
 */
static char    *
find_token(const char *fname, const char *token, char *buf)
{
	FILE           *fp;
	int             len = strlen(token);
	char           *value = NULL;
#ifdef linux

	fp = fopen(fname, "r");
	if (!fp) {
		perror(fname);
		return NULL;
	}
	while (!feof(fp) && !value) {
		/*
		 * process line-by-line 
		 */
		if (fgets(buf, BUFSIZ, fp) == NULL) {
			fprintf(stderr, "error reading %s\n", fname);
			buf = NULL;
			break;
		}
		if (strncmp(buf, token, len) == 0) {
			/*
			 * look for ':' 
			 */
			buf += len;
			while (*buf && *buf != ':') {
				buf++;
			}
			if (*buf) {
				value = buf + 1;	/* skip ':' */
			}
		}
	}
	fclose(fp);
#endif

	return value;
}



/*
 * cpu clock frequency (in hz)
 * return error if not found
 */
int
r_cpu_freq(u_int64_t * freq)
{
	char            buf[BUFSIZ];
	double          f_mhz;
	char           *bufp;
	int             err = 1;

#ifdef linux
	/*
	 * XXX 
	 */
	if ((bufp = find_token("/proc/cpuinfo", "cpu MHz", buf)) != NULL) {
		f_mhz = strtod(bufp, NULL);
		*freq = f_mhz * 1000000;
		err = 0;
	}
#endif
	return err;
}

/*
 * amount of available memory (in KB) 
 * returns error if not found
 */
int
r_freemem(u_int64_t * mem)
{
	int             err = 1;
	char           *bufp,
	               *p,
	                buf[BUFSIZ];

#ifdef linux
	/*
	 * XXX 
	 */
	if ((bufp = find_token("/proc/meminfo", "MemFree", buf)) != NULL) {
		*mem = strtoll(bufp, &p, 0);
		if (*p && *(p + 1) == 'k') {
			*mem *= 1000;
		}
		err = 0;
	}
#endif

	return err;
}
