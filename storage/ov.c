/*  $Id$
**
**  The implementation of the overview API.
**
**  This code handles calls to the overview API by passing them along to the
**  appropriate underlying overview method, as well as implementing those
**  portions of the overview subsystem that are independent of storage
**  method.
*/

#include "config.h"
#include "clibrary.h"
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <syslog.h>

#include "inn/innconf.h"
#include "macros.h"
#include "libinn.h"
#include "ov.h"
#include "ovinterface.h"
#include "ovmethods.h"

/* FIXME: The following variables are shared between this file and expire.c.
   This should be cleaned up with a better internal interface. */
static bool	OVdelayrm;
static OV_METHOD	ov;

bool
OVopen(int mode)
{
    int	i;
    bool val;
    char *p;

    if (ov.open)
	/* already opened */
	return TRUE;

    /* if innconf isn't already read in, do so. */
    if (innconf == NULL)
        if (!innconf_read(NULL))
            return FALSE;
    if (!innconf->enableoverview) {
	syslog(L_FATAL, "enableoverview is not true");
	fprintf(stderr, "enableoverview is not true\n");
	return FALSE;
    }
    if (innconf->ovmethod == NULL) {
	syslog(L_FATAL, "ovmethod is not defined");
	fprintf(stderr, "ovmethod is not defined\n");
	return FALSE;
    }
    for (i=0;i<NUM_OV_METHODS;i++) {
	if (!strcmp(innconf->ovmethod, ov_methods[i].name))
	    break;
    }
    if (i == NUM_OV_METHODS) {
	syslog(L_FATAL, "%s is not found for ovmethod", innconf->ovmethod);
	fprintf(stderr, "%s is not found for ovmethod\n", innconf->ovmethod);
	return FALSE;
    }
    ov = ov_methods[i];
    val = (*ov.open)(mode);
    if (atexit(OVclose) < 0) {
	OVclose();
	return FALSE;
    }
    if (innconf->ovgrouppat != NULL) {
	for (i = 1, p = innconf->ovgrouppat; *p && (p = strchr(p+1, ',')); i++);
	OVnumpatterns = i;
	OVpatterns = NEW(char *, OVnumpatterns);
	for (i = 0, p = strtok(innconf->ovgrouppat, ","); p != NULL && i <= OVnumpatterns ; i++, p = strtok(NULL, ","))
	    OVpatterns[i] = COPY(p);
	if (i != OVnumpatterns) {
	    syslog(L_FATAL, "extra ',' in pattern");
	    fprintf(stderr, "extra ',' in pattern");
	    return FALSE;
	}
    }
    return val;
}

bool
OVgroupstats(char *group, int *lo, int *hi, int *count, int *flag)
{
    if (!ov.open) {
	/* must be opened */
	syslog(L_ERROR, "ovopen must be called first");
	fprintf(stderr, "ovopen must be called first");
	return FALSE;
    }
    return ((*ov.groupstats)(group, lo, hi, count, flag));
}

bool
OVgroupadd(char *group, ARTNUM lo, ARTNUM hi, char *flag)
{
    /* lomark should never be changed in each ovmethod if lo is 0 */
    if (!ov.open) {
	/* must be opened */
	syslog(L_ERROR, "ovopen must be called first");
	fprintf(stderr, "ovopen must be called first");
	return FALSE;
    }
    return ((*ov.groupadd)(group, lo, hi, flag));
}

bool
OVgroupdel(char *group)
{
    if (!ov.open) {
	/* must be opened */
	syslog(L_ERROR, "ovopen must be called first");
	fprintf(stderr, "ovopen must be called first");
	return FALSE;
    }
    return ((*ov.groupdel)(group));
}

OVADDRESULT
OVadd(TOKEN token, char *data, int len, time_t arrived, time_t expires)
{
    char		*next, *nextcheck;
    static char		*xrefdata, *patcheck, *overdata;
    char                *xrefstart = NULL;
    char		*xrefend;
    static int		xrefdatalen = 0, overdatalen = 0;
    bool		found = FALSE;
    int			xreflen;
    int			i;
    char		*group;
    ARTNUM		artnum;

    if (!ov.open) {
	/* must be opened */
	syslog(L_ERROR, "ovopen must be called first");
	fprintf(stderr, "ovopen must be called first");
	return OVADDFAILED;
    }

    /*
     * find last Xref: in the overview line.  Note we need to find the *last*
     * Xref:, since there have been corrupted articles on Usenet with Xref:
     * fragments stuck in other header lines.  The last Xref: is guaranteed
     * to be from our server.
     */

    for (next = data; ((len - (next - data)) > 6 ) && ((next = memchr(next, 'X', len - (next - data))) != NULL); ) {
        if (memcmp(next, "Xref: ", 6) == 0) {
            found =  TRUE;
            xrefstart = next;
        }
        next++;
    }

    if (!found)
        return OVADDFAILED;

    next = xrefstart;
    for (i = 0; (i < 2) && (next < (data + len)); i++) {
        if ((next = memchr(next, ' ', len - (next - data))) == NULL)
            return OVADDFAILED;
        next++;
    }
    xreflen = len - (next - data);

    /*
     * If there are other fields beyond Xref in overview, then
     * we must find Xref's end, or data following is misinterpreted.
     */
    if ((xrefend = memchr(next, '\t', xreflen)) != NULL)
	xreflen = xrefend - next;

    if (xrefdatalen == 0) {
        xrefdatalen = BIG_BUFFER;
        xrefdata = NEW(char, xrefdatalen);
        if (innconf->ovgrouppat != NULL)
            patcheck = NEW(char, xrefdatalen);
    }
    if (xreflen > xrefdatalen) {
        xrefdatalen = xreflen;
        RENEW(xrefdata, char, xrefdatalen + 1);
        if (innconf->ovgrouppat != NULL)
            RENEW(patcheck, char, xrefdatalen + 1);
    }
    if (overdatalen == 0) {
	overdatalen = BIG_BUFFER;
	overdata = NEW(char, overdatalen);
    }
    if (len + 16 > overdatalen) {
	overdatalen = len + 16;
	RENEW(overdata, char, overdatalen);
    }

    if (innconf->ovgrouppat != NULL) {
        memcpy(patcheck, next, xreflen);
        patcheck[xreflen] = '\0';
        for (group = patcheck; group && *group; group = memchr(nextcheck, ' ', xreflen - (nextcheck - patcheck))) {
            while (isspace((int)*group))
                group++;
            if ((nextcheck = memchr(group, ':', xreflen - (patcheck - group))) == NULL)
                return OVADDFAILED;
            *nextcheck++ = '\0';
            if (!OVgroupmatch(group)) {
                if (!SMprobe(SELFEXPIRE, &token, NULL) && innconf->groupbaseexpiry)
                    /* this article will never be expired, since it does not
                       have self expiry function in stored method and
                       groupbaseexpiry is true */
                    return OVADDFAILED;
                return OVADDGROUPNOMATCH;
            }
        }
    }
    memcpy(xrefdata, next, xreflen);
    xrefdata[xreflen] = '\0';
    for (group = xrefdata; group && *group; group = memchr(next, ' ', xreflen - (next - xrefdata))) {
        /* Parse the xref part into group name and article number */
        while (isspace((int)*group))
            group++;
        if ((next = memchr(group, ':', xreflen - (group - xrefdata))) == NULL)
            return OVADDFAILED;
        *next++ = '\0';
        artnum = atoi(next);
        if (artnum <= 0)
            continue;

        sprintf(overdata, "%ld\t", artnum);
        i = strlen(overdata);
        memcpy(overdata + i, data, len);
        i += len;
        memcpy(overdata + i, "\r\n", 2);
        i += 2;

	if(! (*ov.add)(group, artnum, token, overdata, i, arrived, expires))
	    return OVADDFAILED;
    }

    return OVADDCOMPLETED;
}

bool
OVcancel(TOKEN token)
{
    if (!ov.open) {
	/* must be opened */
	syslog(L_ERROR, "ovopen must be called first");
	fprintf(stderr, "ovopen must be called first");
	return FALSE;
    }
    return ((*ov.cancel)(token));
}

void *
OVopensearch(char *group, int low, int high)
{
    if (!ov.open) {
	/* must be opened */
	syslog(L_ERROR, "ovopen must be called first");
	fprintf(stderr, "ovopen must be called first");
	return FALSE;
    }
    return ((*ov.opensearch)(group, low, high));
}

bool
OVsearch(void *handle, ARTNUM *artnum, char **data, int *len, TOKEN *token,
         time_t *arrived)
{
    if (!ov.open) {
	/* must be opened */
	syslog(L_ERROR, "ovopen must be called first");
	fprintf(stderr, "ovopen must be called first");
	return FALSE;
    }
    return ((*ov.search)(handle, artnum, data, len, token, arrived));
}

void
OVclosesearch(void *handle)
{
    if (!ov.open) {
	/* must be opened */
	syslog(L_ERROR, "ovopen must be called first");
	fprintf(stderr, "ovopen must be called first");
	return;
    }
    (*ov.closesearch)(handle);
    return;
}

bool
OVgetartinfo(char *group, ARTNUM artnum, TOKEN *token)
{
    if (!ov.open) {
	/* must be opened */
	syslog(L_ERROR, "ovopen must be called first");
	fprintf(stderr, "ovopen must be called first");
	return FALSE;
    }
    return ((*ov.getartinfo)(group, artnum, token));
}

bool
OVexpiregroup(char *group, int *lo, struct history *h)
{
    if (!ov.open) {
	/* must be opened */
	syslog(L_ERROR, "ovopen must be called first");
	fprintf(stderr, "ovopen must be called first");
	return FALSE;
    }
    return ((*ov.expiregroup)(group, lo, h));
}

bool
OVctl(OVCTLTYPE type, void *val)
{
    if (!ov.open) {
	/* must be opened */
	syslog(L_ERROR, "ovopen must be called first");
	fprintf(stderr, "ovopen must be called first");
	return FALSE;
    }
    switch (type) {
    case OVGROUPBASEDEXPIRE:
	if (!innconf->groupbaseexpiry) {
	    syslog(L_ERROR, "OVGROUPBASEDEXPIRE is not allowed if groupbaseexpiry if false");
	    fprintf(stderr, "OVGROUPBASEDEXPIRE is not allowed if groupbaseexpiry if false");
	    return FALSE;
	}
	if (((OVGE *)val)->delayrm) {
	    if ((((OVGE *)val)->filename == NULL) || (strlen(((OVGE *)val)->filename) == 0)) {
		syslog(L_ERROR, "file name must be specified");
		fprintf(stderr, "file name must be specified");
	  	return FALSE;
	    }
	    if ((EXPunlinkfile = fopen(((OVGE *)val)->filename, "w")) == NULL) {
		syslog(L_ERROR, "fopen: %s failed: %m", ((OVGE *)val)->filename);
		fprintf(stderr, "fopen: %s failed: %s", ((OVGE *)val)->filename, 
			      strerror(errno));
		return FALSE;
	    }
	}
	OVdelayrm = ((OVGE *)val)->delayrm;
	OVusepost = ((OVGE *)val)->usepost;
	OVrealnow = ((OVGE *)val)->now;
	OVnow = ((OVGE *)val)->now + (time_t)((OVGE *)val)->timewarp;
	OVquiet = ((OVGE *)val)->quiet;
	OVkeep = ((OVGE *)val)->keep;
	OVearliest = ((OVGE *)val)->earliest;
	OVignoreselfexpire = ((OVGE *)val)->ignoreselfexpire;
	return TRUE;
    case OVSTATALL:
	OVstatall = *(bool *)val;
	return TRUE;
    default:
	return ((*ov.ctl)(type, val));
    }
}

void
OVclose(void)
{
    if (!ov.open)
	return;
    (*ov.close)();
    memset(&ov, '\0', sizeof(ov));
    OVEXPcleanup();
}
