/*  $Id$
**
**  Newsgroups and the active file.
*/

#include "config.h"
#include "clibrary.h"

#include "inn/innconf.h"
#include "nnrpd.h"
#include "inn/ov.h"

extern bool CMDgetrange(int ac, char *av[], ARTRANGE *rp, bool *DidReply);
extern bool CMDisrange(char *string);

/*
**  Change to or list the specified newsgroup.  If invalid, stay in the old
**  group.
*/
void
CMDgroup(int ac, char *av[])
{
    static char		NOSUCHGROUP[] = NNTP_NOSUCHGROUP;
    ARTNUM              i;
    int                 low, high;
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
        if (PERMspecified)
	    Reply("%d Permission denied\r\n", NNTP_ERR_ACCESS);
        else
            Reply("%d Authentication required\r\n", NNTP_FAIL_AUTH_NEEDED);
	return;
    }

    /* Parse arguments. */
    if (ac == 1) {
	if (GRPcur == NULL) {
	    Printf("%d No group specified\r\n", NNTP_FAIL_NO_GROUP);
	    return;
	} else {
	    group = xstrdup(GRPcur);
	}
    } else {
	group = xstrdup(av[1]);
    }

    /* Check whether the second argument is valid. */
    if (ac == 3 && !CMDisrange(av[2])) {
        Reply("%d Syntax error\r\n", NNTP_ERR_SYNTAX);
        return;
    }

    /* FIXME: Temporarily work around broken API. */
    if (!OVgroupstats(group, &low, &high, &count, NULL)) {
        Reply("%s %s\r\n", NOSUCHGROUP, group);
        free(group);
        return;
    }
    ARTlow = low;
    ARThigh = high;

#ifdef DO_PYTHON
    if (PY_use_dynamic) {
        char    *reply;

	/* Authorize user using Python module method dynamic*/
	if (PY_dynamic(PERMuser, group, false, &reply) < 0) {
	    syslog(L_NOTICE, "PY_dynamic(): authorization skipped due to no Python dynamic method defined.");
	} else {
	    if (reply != NULL) {
	        syslog(L_TRACE, "PY_dynamic() returned a refuse string for user %s at %s who wants to read %s: %s", PERMuser, Client.host, group, reply);
		Reply("%d %s\r\n", NNTP_ERR_ACCESS, reply);
		free(group);
                free(reply);
		return;
	    }
	}
    }
#endif /* DO_PYTHON */

    if (!hookpresent) {
        if (PERMspecified) {
            grplist[0] = group;
            grplist[1] = NULL;
            if (!PERMmatch(PERMreadlist, grplist)) {
                Reply("%d Permission denied\r\n", NNTP_ERR_ACCESS);
                free(group);
                return;
            }
        } else {
            Reply("%d Authentication required\r\n", NNTP_FAIL_AUTH_NEEDED);
            free(group);
            return;
        }
    }

    /* Close out any existing article, report group stats. */
    ARTclose();
    GRPreport();

    /* Doing a GROUP command? */
    if (strcasecmp(av[0], "GROUP") == 0) {
	if (count == 0) {
            if (ARTlow == 0)
                ARTlow = 1;
	    Reply("%d 0 %lu %lu %s\r\n", NNTP_OK_GROUP, ARTlow, ARTlow-1, group);
        } else {
	    /* if we are an NFS reader, check the last nfsreaderdelay
	     * articles in the group to see if they arrived in the
	     * last nfsreaderdelay (default 60) seconds.  If they did,
	     * don't report them as we don't want them to appear too
	     * soon. */
	    if (innconf->nfsreader) {
		ARTNUM low, prev;
		time_t now, arrived;

		time(&now);
                /* We assume that during the last nfsreaderdelay seconds,
                 * we did not receive more than 1 article per second. */
		if (ARTlow + innconf->nfsreaderdelay > ARThigh)
		    low = ARTlow;
		else
		    low = ARThigh - innconf->nfsreaderdelay;
		handle = OVopensearch(group, low, ARThigh);
		if (!handle) {
		    Reply("%d group disappeared\r\n", NNTP_FAIL_ACTION);
		    free(group);
		    return;
		}
		prev = low;
		while (OVsearch(handle, &i, NULL, NULL, NULL, &arrived)) {
		    if (arrived + innconf->nfsreaderdelay > now) {
			ARThigh = prev;
                        /* No need to update the count since it is only
                         * an estimate but make sure it is not too high. */
                        if ((unsigned int)count > ARThigh - ARTlow)
                            count = ARThigh - ARTlow + 1;
			break;
		    }
		    prev = i;
		}
		OVclosesearch(handle);
	    }
	    Reply("%d %d %lu %lu %s\r\n", NNTP_OK_GROUP, count, ARTlow,
                  ARThigh, group);
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
        /* Must be doing a LISTGROUP command.  We used to just return
           something bland here ("Article list follows"), but reference NNTP
           returns the same data as GROUP does and since we have it all
           available it shouldn't hurt to return the same thing. */
        ARTRANGE range;
        bool DidReply;

        /* Parse the range. */
        if (ac == 3) {
            /* CMDgetrange() expects av[1] to contain the range.
             * It is av[2] for LISTGROUP.
             * We already know that GRPcur exists. */
            if (!CMDgetrange(ac, av + 1, &range, &DidReply)) {
                if (DidReply) {
                    free(group);
                    return;
                }
            }
        } else {
            range.Low = ARTlow;
            range.High = ARThigh;
        }

        if (count == 0) {
            if (ARTlow == 0)
                ARTlow = 1;
            Reply("%d 0 %lu %lu %s\r\n", NNTP_OK_GROUP, ARTlow, ARTlow-1, group);
            Printf(".\r\n");
        } else {
            Reply("%d %d %lu %lu %s\r\n", NNTP_OK_GROUP, count, ARTlow,
                  ARThigh, group);
            /* If OVopensearch() is restricted to the range, it returns NULL
             * in case there isn't any article within the range.  We already
             * know that the group exists. */
            if ((handle = OVopensearch(group, range.Low, range.High)) != NULL) {
                while (OVsearch(handle, &i, NULL, NULL, &token, NULL)) {
                    if (PERMaccessconf->nnrpdcheckart && !ARTinstorebytoken(token))
                        continue;
                    Printf("%lu\r\n", i);
                }

                OVclosesearch(handle);
            }
            Printf(".\r\n");
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
    }
    free(group);
}


/*
**  Report on the number of articles read in the group, and clear the count.
*/
void
GRPreport(void)
{
    char		buff[SPOOLNAMEBUFF];
    char		repbuff[1024];

    if (GRPcur) {
	strlcpy(buff, GRPcur, sizeof(buff));
	syslog(L_NOTICE, "%s group %s %lu", Client.host, buff, GRParticles);
	GRParticles = 0;
	repbuff[0]='\0';
    }
}


/*
**  Used by ANU-News clients.
*/
void
CMDxgtitle(int ac, char *av[])
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
	    Printf("%d No group specified\r\n", NNTP_FAIL_XGTITLE);
	    return;
	}
	p = GRPcur;
    }
    else
	p = av[1];

    if (!PERMspecified) {
	Printf("%d list follows\r\n", NNTP_OK_XGTITLE);
	Printf(".\r\n");
	return;
    }

    /* Open the file, get ready to scan. */
    if ((qp = QIOopen(NEWSGROUPS)) == NULL) {
	syslog(L_ERROR, "%s cant open %s %m", Client.host, NEWSGROUPS);
	Printf("%d Can't open %s\r\n", NNTP_FAIL_XGTITLE, NEWSGROUPS);
	return;
    }
    Printf("%d list follows\r\n", NNTP_OK_XGTITLE);

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
