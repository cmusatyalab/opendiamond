/*
 *
 *
 *                          Diamond 1.0
 * 
 *            Copyright (c) 2002-2004, Intel Corporation
 *                         All Rights Reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 *    * Neither the name of Intel nor the names of its contributors may
 *      be used to endorse or promote products derived from this software 
 *      without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "rstat.h"

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
				value = buf + 1;    /* skip ':' */
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
