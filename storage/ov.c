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

#include "inn/innconf.h"
#include "inn/messages.h"
#include "inn/wire.h"
#include "inn/vector.h"
#include "inn/libinn.h"
#include "inn/ov.h"
#include "ovinterface.h"
#include "ovmethods.h"

static bool             OVdelayrm;
static OV_METHOD	ov;

time_t	OVrealnow;
bool    OVstatall;

bool
OVopen(int mode)
{
    int	i;
    bool val;

    if (ov.open)
	/* already opened */
	return true;

    /* if innconf isn't already read in, do so. */
    if (innconf == NULL)
        if (!innconf_read(NULL))
            return false;
    if (!innconf->enableoverview) {
	warn("enableoverview is not true");
	return false;
    }
    if (innconf->ovmethod == NULL) {
        warn("ovmethod is not defined");
	return false;
    }
    for (i=0;i<NUM_OV_METHODS;i++) {
	if (!strcmp(innconf->ovmethod, ov_methods[i].name))
	    break;
    }
    if (i == NUM_OV_METHODS) {
        warn("%s is not found for ovmethod", innconf->ovmethod);
	return false;
    }
    ov = ov_methods[i];
    val = (*ov.open)(mode);
    if (atexit(OVclose) < 0) {
	OVclose();
	return false;
    }
    return val;
}

bool
OVgroupstats(char *group, int *lo, int *hi, int *count, int *flag)
{
    if (!ov.open) {
	/* must be opened */
        warn("ovopen must be called first");
	return false;
    }
    return ((*ov.groupstats)(group, lo, hi, count, flag));
}

bool
OVgroupadd(char *group, ARTNUM lo, ARTNUM hi, char *flag)
{
    /* lomark should never be changed in each ovmethod if lo is 0 */
    if (!ov.open) {
	/* must be opened */
        warn("ovopen must be called first");
	return false;
    }
    return ((*ov.groupadd)(group, lo, hi, flag));
}

bool
OVgroupdel(char *group)
{
    if (!ov.open) {
	/* must be opened */
        warn("ovopen must be called first");
	return false;
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
    bool		found = false;
    int			xreflen;
    int			i;
    char		*group;
    ARTNUM		artnum;
    enum uwildmat       groupmatch;

    if (!ov.open) {
	/* Must be opened. */
        warn("ovopen must be called first");
	return OVADDFAILED;
    }

    /*
     * Find last Xref: in the overview line.  Note we need to find the *last*
     * Xref:, since there have been corrupted articles on Usenet with Xref:
     * fragments stuck in other header lines.  The last Xref: is guaranteed
     * to be from our server.
     */

    for (next = data; ((len - (next - data)) > 6 ) && ((next = memchr(next, 'X', len - (next - data))) != NULL); ) {
        /* Check that Xref: is at the beginning of an overview field. */
        if (memcmp(next, "Xref: ", 6) == 0 && next != data && next[-1] == '\t') {
            found =  true;
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
     * If there are other fields beyond Xref: in overview, then
     * we must find Xref:'s end, or data following is misinterpreted.
     */
    if ((xrefend = memchr(next, '\t', xreflen)) != NULL)
	xreflen = xrefend - next;

    if (xrefdatalen == 0) {
        xrefdatalen = BIG_BUFFER;
        xrefdata = xmalloc(xrefdatalen);
        if (innconf->ovgrouppat != NULL)
            patcheck = xmalloc(xrefdatalen);
    }
    if (xreflen > xrefdatalen) {
        xrefdatalen = xreflen;
        xrefdata = xrealloc(xrefdata, xrefdatalen + 1);
        if (innconf->ovgrouppat != NULL)
            patcheck = xrealloc(patcheck, xrefdatalen + 1);
    }
    if (overdatalen == 0) {
	overdatalen = BIG_BUFFER;
	overdata = xmalloc(overdatalen);
    }
    if (len + 16 > overdatalen) {
	overdatalen = len + 16;
        overdata = xrealloc(overdata, overdatalen);
    }

    if (innconf->ovgrouppat != NULL) {
        memcpy(patcheck, next, xreflen);
        patcheck[xreflen] = '\0';
        for (group = patcheck; group && *group; group = memchr(nextcheck, ' ', xreflen - (nextcheck - patcheck))) {
            while (isspace((int)*group))
                group++;
            if ((nextcheck = memchr(group, ':', xreflen - (group - patcheck))) == NULL)
                return OVADDFAILED;
            *nextcheck++ = '\0';

            groupmatch = uwildmat_poison(group, innconf->ovgrouppat);
            if (groupmatch == UWILDMAT_POISON) {
                return OVADDGROUPNOMATCH;
            } else if (groupmatch == UWILDMAT_FAIL) {
                if (!SMprobe(SELFEXPIRE, &token, NULL) && innconf->groupbaseexpiry) {
                    /* This article will never be expired, since it does not
                     * have self expiry function in stored method and
                     * groupbaseexpiry is true. */
                    return OVADDFAILED;
                }
            }
        }
    }
    memcpy(xrefdata, next, xreflen);
    xrefdata[xreflen] = '\0';
    for (group = xrefdata; group && *group; group = memchr(next, ' ', xreflen - (next - xrefdata))) {
        /* Parse the Xref: part into group name and article number. */
        while (isspace((int)*group))
            group++;
        if ((next = memchr(group, ':', xreflen - (group - xrefdata))) == NULL)
            return OVADDFAILED;
        *next++ = '\0';
        artnum = atoi(next);
        if (artnum <= 0)
            continue;

        /* Skip overview generation according to ovgrouppat. */
        if (innconf->ovgrouppat != NULL
            && uwildmat_poison(group, innconf->ovgrouppat) != UWILDMAT_MATCH) {
            continue;
        }

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
    ARTHANDLE *art;
    const char *xref, *xrefend, *group;
    size_t xreflen, i;
    char *xref_copy, *p, *end;
    ARTNUM artnum;
    struct cvector *groups;

    if (!ov.open) {
        /* must be opened */
        warn("ovopen must be called first");
        return false;
    }

    /* There's no easy way to go from a token to the group and article number
       pairs that we need to do this.  Retrieve the article and find the Xref
       header and then parse it to figure out what to cancel.  Articles
       without Xref headers lose. */
    art = SMretrieve(token, RETR_HEAD);
    if (art == NULL)
        return false;
    xref = wire_findheader(art->data, art->len, "Xref", true);
    if (xref == NULL)
        goto fail;
    xrefend = wire_endheader(xref, art->data + art->len - 1);
    if (xrefend == NULL)
        goto fail;
    xreflen = xrefend - xref + 1;
    xref_copy = xstrndup(xref, xreflen);
    SMfreearticle(art);
    groups = cvector_split_space(xref_copy, NULL);
    for (i = 0; i < groups->count; i++) {
        group = groups->strings[i];
        p = (char *) strchr(group, ':');
        if (p == NULL || p == group || p[1] == '-')
            continue;
        *p = '\0';
        errno = 0;
        artnum = strtoul(p + 1, &end, 10);
        if (artnum == 0 || *end != '\0' || errno == ERANGE)
            continue;

        /* Don't worry about the return status; the article may have already
           expired out of some or all of the groups. */
        (*ov.cancel)(group, artnum);
    }
    free(xref_copy);
    cvector_free(groups);
    return true;

fail:
    SMfreearticle(art);
    return false;
}

void *
OVopensearch(char *group, int low, int high)
{
    if (!ov.open) {
	/* must be opened */
        warn("ovopen must be called first");
	return false;
    }
    return ((*ov.opensearch)(group, low, high));
}

bool
OVsearch(void *handle, ARTNUM *artnum, char **data, int *len, TOKEN *token,
         time_t *arrived)
{
    if (!ov.open) {
	/* must be opened */
	warn("ovopen must be called first");
	return false;
    }
    return ((*ov.search)(handle, artnum, data, len, token, arrived));
}

void
OVclosesearch(void *handle)
{
    if (!ov.open) {
	/* must be opened */
	warn("ovopen must be called first");
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
	warn("ovopen must be called first");
	return false;
    }
    return ((*ov.getartinfo)(group, artnum, token));
}

bool
OVexpiregroup(char *group, int *lo, struct history *h)
{
    if (!ov.open) {
	/* must be opened */
	warn("ovopen must be called first");
	return false;
    }
    return ((*ov.expiregroup)(group, lo, h));
}

bool
OVctl(OVCTLTYPE type, void *val)
{
    if (!ov.open) {
	/* must be opened */
	warn("ovopen must be called first");
	return false;
    }
    switch (type) {
    case OVGROUPBASEDEXPIRE:
	if (!innconf->groupbaseexpiry) {
            warn("OVGROUPBASEDEXPIRE is not allowed if groupbaseexpiry is"
                 " false");
	    return false;
	}
	if (((OVGE *)val)->delayrm) {
	    if ((((OVGE *)val)->filename == NULL) || (strlen(((OVGE *)val)->filename) == 0)) {
                warn("file name must be specified");
	  	return false;
	    }
	    if ((EXPunlinkfile = fopen(((OVGE *)val)->filename, "w")) == NULL) {
                syswarn("fopen: %s failed", ((OVGE *)val)->filename);
		return false;
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
	return true;
    case OVSTATALL:
	OVstatall = *(bool *)val;
	return true;
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
