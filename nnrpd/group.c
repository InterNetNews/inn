/*  $Revision$
**
**  Newsgroups and the active file.
*/
#include "config.h"
#include "clibrary.h"
#include <netinet/in.h>
#include <sys/mman.h>

#include "nnrpd.h"
#include "ov.h"

/*
**  Change to or list the specified newsgroup.  If invalid, stay in the old
**  group.
*/
FUNCTYPE CMDgroup(int ac, char *av[])
{
    static char		NOSUCHGROUP[] = NNTP_NOSUCHGROUP;
    ARTNUM              i;
    char		*grplist[2];
    char		*group;
    void                *handle;
    TOKEN               token;
    int                 count;

    if (!PERMcanread) {
	Reply("%s\r\n", NOACCESS);
	return;
    }

    /* Parse arguments. */
    if (ac == 1) {
	    Printf("%d No group specified\r\n", NNTP_XGTITLE_BAD);
	    return;
    } else {
	group = av[1];
    }
    
    if (!OVgroupstats(group, &ARTlow, &ARThigh, &count, NULL)) {
	Reply("%s %s\r\n", NOSUCHGROUP, group);
	return;
    }

#ifdef DO_PYTHON
    if (innconf->nnrppythonauth) {
        char    *reply;

	/* Authorize user at a Python authorization module */
	if (PY_authorize(ClientHost, ClientIp, ServerHost, PERMuser, group, FALSE, &reply) < 0) {
	    syslog(L_NOTICE, "PY_authorize(): authorization skipped due to no Python authorization method defined.");
	} else {
	    if (reply != NULL) {
	        syslog(L_TRACE, "PY_authorize() returned a refuse string for user %s at %s who wants to read %s: %s", PERMuser, ClientHost, group, reply);
		Reply("%d %s\r\n", NNTP_ACCESS_VAL, reply);
		return;
	    }
	}
    }
#endif /* DO_PYTHON */

    /* If permission is denied, pretend group doesn't exist. */
    if (PERMspecified) {
	grplist[0] = group;
	grplist[1] = NULL;
	if (!PERMmatch(PERMreadlist, grplist)) {
	    Reply("%s %s\r\n", NOSUCHGROUP, group);
	    return;
	}
    } else {
	Reply("%s %s\r\n", NOSUCHGROUP, group);
	return;
    }

    /* Close out any existing article, report group stats. */
    ARTclose();
    GRPreport();
    HIScheck();

    /* Doing a "group" command? */
    if (caseEQ(av[0], "group")) {
	if (count == 0)
	    Reply("%d 0 0 0 %s\r\n", NNTP_GROUPOK_VAL, group);
	else
	    Reply("%d %d %ld %ld %s\r\n",
		NNTP_GROUPOK_VAL,
		count, ARTlow, ARThigh, group);
	GRPcount++;
	ARTnumber = ARTlow;
	if (GRPcur)
	    DISPOSE(GRPcur);
	GRPcur = COPY(group);
    } else {
	/* Must be doing a "listgroup" command. */
	if ((handle = OVopensearch(group, ARTlow, ARThigh)) != NULL) {
	Reply("%d Article list follows\r\n", NNTP_GROUPOK_VAL);
	    while (OVsearch(handle, &i, NULL, NULL, &token, NULL)) {
		if (PERMaccessconf->nnrpdcheckart && !ARTinstorebytoken(token))
		    continue;
		Printf("%ld\r\n", i);
	    }
	    OVclosesearch(handle);
	Printf(".\r\n");
	    GRPcount++;
	    ARTnumber = ARTlow;
	    if (GRPcur)
		DISPOSE(GRPcur);
	    GRPcur = COPY(group);
	} else {
	    Reply("%s %s\r\n", NOSUCHGROUP, group);
	}
    }
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
	(void)strcpy(buff, GRPcur);
	syslog(L_NOTICE, "%s group %s %ld", ClientHost, buff, GRParticles);
	GRParticles = 0;
	repbuff[0]='\0';
    }
}


/*
**  Used by ANU-News clients.
*/
FUNCTYPE
CMDxgtitle(ac, av)
    int			ac;
    char		*av[];
{
    register QIOSTATE	*qp;
    register char	*line;
    register char	*p;
    register char	*q;
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
	if (wildmat(line, p)) {
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
