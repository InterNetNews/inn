/*  $Id$
**
**  Newsgroups and the active file.
*/

#include "config.h"
#include "clibrary.h"

#include "inn/innconf.h"
#include "nnrpd.h"
#include "ov.h"

/*
**  Change to or list the specified newsgroup.  If invalid, stay in the old
**  group.
*/
void CMDgroup(int ac, char *av[])
{
    static char		NOSUCHGROUP[] = NNTP_NOSUCHGROUP;
    ARTNUM              i;
    char		*grplist[2];
    char		*group;
    void                *handle;
    TOKEN               token;
    int                 count;
    bool		boolval;
    bool                hookpresent = false;

#ifdef DO_PYTHON
    hookpresent = PY_use_dynamic;
#endif /* DO_PYTHON */

    if (!hookpresent && !PERMcanread) {
	Reply("%s\r\n", NOACCESS);
	return;
    }

    /* Parse arguments. */
    if (ac == 1) {
	if (GRPcur == NULL) {
	    Printf("%d No group specified\r\n", NNTP_XGTITLE_BAD);
	    return;
	} else {
	    group = xstrdup(GRPcur);
	}
    } else {
	group = xstrdup(av[1]);
    }
    
    if (!OVgroupstats(group, &ARTlow, &ARThigh, &count, NULL)) {
	Reply("%s %s\r\n", NOSUCHGROUP, group);
	free(group);
	return;
    }

#ifdef DO_PYTHON
    if (PY_use_dynamic) {
        char    *reply;

	/* Authorize user using Python module method dynamic*/
	if (PY_dynamic(PERMuser, group, false, &reply) < 0) {
	    syslog(L_NOTICE, "PY_dynamic(): authorization skipped due to no Python dynamic method defined.");
	} else {
	    if (reply != NULL) {
	        syslog(L_TRACE, "PY_dynamic() returned a refuse string for user %s at %s who wants to read %s: %s", PERMuser, ClientHost, group, reply);
		Reply("%d %s\r\n", NNTP_ACCESS_VAL, reply);
		free(group);
                free(reply);
		return;
	    }
	}
    }
#endif /* DO_PYTHON */

    /* If permission is denied, pretend group doesn't exist. */
    if (!hookpresent) {
        if (PERMspecified) {
            grplist[0] = group;
            grplist[1] = NULL;
            if (!PERMmatch(PERMreadlist, grplist)) {
	         Reply("%s %s\r\n", NOSUCHGROUP, group);
                 free(group);
                 return;
            }
        } else {
            Reply("%s %s\r\n", NOSUCHGROUP, group);
            free(group);
            return;
        }
    }

    /* Close out any existing article, report group stats. */
    ARTclose();
    GRPreport();

    /* Doing a "group" command? */
    if (strcasecmp(av[0], "group") == 0) {
	if (count == 0)
	    Reply("%d 0 0 0 %s\r\n", NNTP_GROUPOK_VAL, group);
	else {
	    /* if we're an NFS reader, check the last nfsreaderdelay
	     * articles in the group to see if they arrived in the
	     * last nfsreaderdelay (default 60) seconds. If they did,
	     * don't report them as we don't want them to appear too
	     * soon */
	    if (innconf->nfsreader) {
		ARTNUM low, prev;
		time_t now, arrived;

		time(&now);
		if (ARTlow + innconf->nfsreaderdelay > ARThigh)
		    low = ARTlow;
		else
		    low = ARThigh - innconf->nfsreaderdelay;
		handle = OVopensearch(group, low, ARThigh);
		if (!handle) {
		    Reply("%d group disappeared\r\n", NNTP_TEMPERR_VAL);
		    free(group);
		    return;
		}
		prev = low;
		while (OVsearch(handle, &i, NULL, NULL, NULL, &arrived)) {
		    if (arrived + innconf->nfsreaderdelay > now) {
			ARThigh = prev;
			break;
		    }
		    prev = i;
		}
		OVclosesearch(handle);
	    }
	    Reply("%d %d %ld %ld %s\r\n",
		NNTP_GROUPOK_VAL,
		count, ARTlow, ARThigh, group);
	}
	GRPcount++;
	ARTnumber = ARTlow;
	if (GRPcur) {
	    if (strcmp(GRPcur, group) != 0) {
		OVctl(OVCACHEFREE, &boolval);
		free(GRPcur);
		GRPcur = xstrdup(group);
	    }
	} else
	    GRPcur = xstrdup(group);
    } else {
	/* Must be doing a "listgroup" command.  We used to just return
           something bland here ("Article list follows"), but reference NNTP
           returns the same data as GROUP does and since we have it all
           available it shouldn't hurt to return the same thing. */
	if ((handle = OVopensearch(group, ARTlow, ARThigh)) != NULL) {
            if (count == 0)
                Reply("%d 0 0 0 %s\r\n", NNTP_GROUPOK_VAL, group);
            else
                Reply("%d %d %ld %ld %s\r\n", NNTP_GROUPOK_VAL, count,
                      ARTlow, ARThigh, group);
	    while (OVsearch(handle, &i, NULL, NULL, &token, NULL)) {
		if (PERMaccessconf->nnrpdcheckart && !ARTinstorebytoken(token))
		    continue;
		Printf("%ld\r\n", i);
	    }
	    OVclosesearch(handle);
	    Printf(".\r\n");
	    GRPcount++;
	    ARTnumber = ARTlow;
	    if (GRPcur) {
		if (strcmp(GRPcur, group) != 0) {
		    OVctl(OVCACHEFREE, &boolval);
		    free(GRPcur);
		    GRPcur = xstrdup(group);
		}
	    } else
		GRPcur = xstrdup(group);
	} else {
	    Reply("%s %s\r\n", NOSUCHGROUP, group);
	}
    }
    free(group);
}


/*
**  Report on the number of articles read in the group, and clear the count.
*/
void
GRPreport()
{
    char		buff[SPOOLNAMEBUFF];
    char		repbuff[1024];

    if (GRPcur) {
	strlcpy(buff, GRPcur, sizeof(buff));
	syslog(L_NOTICE, "%s group %s %ld", ClientHost, buff, GRParticles);
	GRParticles = 0;
	repbuff[0]='\0';
    }
}


/*
**  Used by ANU-News clients.
*/
void
CMDxgtitle(ac, av)
    int			ac;
    char		*av[];
{
    QIOSTATE	*qp;
    char	*line;
    char	*p;
    char	*q;
    char		*grplist[2];
    char		save;

    /* Parse the arguments. */
    if (ac == 1) {
	if (GRPcount == 0) {
	    Printf("%d No group specified\r\n", NNTP_XGTITLE_BAD);
	    return;
	}
	p = GRPcur;
    }
    else
	p = av[1];

    if (!PERMspecified) {
	Printf("%d list follows\r\n", NNTP_XGTITLE_OK);
	Printf(".\r\n");
	return;
    }

    /* Open the file, get ready to scan. */
    if ((qp = QIOopen(NEWSGROUPS)) == NULL) {
	syslog(L_ERROR, "%s cant open %s %m", ClientHost, NEWSGROUPS);
	Printf("%d Can't open %s\r\n", NNTP_XGTITLE_BAD, NEWSGROUPS);
	return;
    }
    Printf("%d list follows\r\n", NNTP_XGTITLE_OK);

    /* Print all lines with matching newsgroup name. */
    while ((line = QIOread(qp)) != NULL) {
	for (q = line; *q && !ISWHITE(*q); q++)
	    continue;
	save = *q;
	*q = '\0';
	if (uwildmat(line, p)) {
	    if (PERMspecified) {
		grplist[0] = line;
		grplist[1] = NULL;
		if (!PERMmatch(PERMreadlist, grplist))
		    continue;
	    }
	    *q = save;
	    Printf("%s\r\n", line);
	}
    }

    /* Done. */
    QIOclose(qp);
    Printf(".\r\n");
}
