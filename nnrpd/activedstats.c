/*
 * Copyright (c) 1996
 *	Joe Greco.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Joe Greco.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JOE GRECO AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL JOE GRECO OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */


#include	<stdio.h>
#include	<sys/time.h>
#include	<sys/types.h>

#include "configdata.h"
#include "clibrary.h"
#include "libinn.h"

/* This breaks past 1024; see comments in sys/param.h - soln: make 64 bits */

#undef FSHIFT
#define FSHIFT  11              /* bits to right of fixed binary point */
#define FSCALE  (1<<FSHIFT)

/* Load average structure. */
struct loadavg {
        u_long  ldavg[3];
	long    fscale;
};      
		 
struct loadavg avgreaders = { {0, 0, 0}, FSCALE};
struct loadavg avgstartups = { {0, 0, 0}, FSCALE};

/*
 * Constants for averages over 1, 5, and 15 minutes
 * when sampling at 5 second intervals.
 */
static u_long cexp[3] = {
        0.9200444146293232 * FSCALE,    /* exp(-1/12) */
        0.9834714538216174 * FSCALE,    /* exp(-1/60) */
        0.9944598480048967 * FSCALE,    /* exp(-1/180) */
};


/*
 * Compute a tenex style load average of a quantity on
 * 1, 5 and 15 minute intervals.
 */
static void
loadav(struct loadavg *avg, int nrun)
{
        register int i;
        register struct proc *p;

        for (i = 0; i < 3; i++)
                avg->ldavg[i] = (cexp[i] * avg->ldavg[i] +
                    nrun * FSCALE * (FSCALE - cexp[i])) >> FSHIFT;
}

/*
 * Call this every half a second or so - every five seconds, it will 
 * calculate loads.
 * 
 * Originally designed for readers.  We don't care for actived and
 * instead we call with loads(0, 1) for requests, and loads (0, 0)
 * for periodic calls.
 *
 * Formerly:
 * When a reader connects:  loads(+1, 1)
 * 		disconn's:  loads(-1, 0)
 */

int
loads(int reader_count, int startup_count)
{
	static int readers = 0;
	static int startups = 0;
	struct timeval tv;
	static int tv_next = 0;

	readers += reader_count;
	startups += startup_count;

	gettimeofday(&tv, NULL);

	if (! tv_next) {
		tv_next = tv.tv_sec + 5;
	}
	if (tv.tv_sec >= tv_next) {
		tv_next += 5;
		loadav(&avgreaders, readers);
		loadav(&avgstartups, startups);
		startups = 0;
		dumploads();
	}
	return(0);
}

int dumploads()
{
	FILE *fp;
	if (! (fp = fopen(cpcatpath(innconf->pathlog, "activedloads"), "w"))) {
		return(-1);
	}
	fprintloads(fp);
	fclose(fp);
	return(0);
}

int
fprintloads(FILE *fp)
{
	double avenrun[3];
	int i;

	for (i = 0; i < 3; i++) {
		avenrun[i] = (double) avgstartups.ldavg[i] / avgstartups.fscale;
	}
	fprintf(fp, "Average ACTIVED Loads: ");
	for (i = 0; i < (sizeof(avenrun) / sizeof(avenrun[0])); i++) {
		if (i > 0)
			fprintf(fp, ",");
		fprintf(fp, " %6.2f", avenrun[i]);
	}
	fprintf(fp, "\n");
}
