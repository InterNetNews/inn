/*  $Revision$
**
**  Newsgroups and the active file.
*/
#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "configdata.h"
#include "clibrary.h"
#include "nnrpd.h"
#include "mydir.h"
#include <sys/mman.h>


/*
**  Newsgroup hashing stuff.  See comments in innd/ng.c.
*/

#define GRP_HASH(Name, p, j)	\
	for (p = Name, j = 0; *p; ) j = (j << 5) + j + *p++
#define GRP_SIZE	512
#define GRP_BUCKET(j)	&GRPtable[j & (GRP_SIZE - 1)]

typedef struct _GRPHASH {
#ifdef NOTDEF
    int		Size;
    int		Used;
    GROUPENTRY	**Groups;
#endif
    GROUPENTRY	*First;
} GRPHASH;


STATIC GRPHASH		GRPtable[GRP_SIZE];
STATIC GROUPENTRY	*GRPentries;
STATIC int		GRPbuckets;
STATIC int		GRPsize;

unsigned int	RARTtable[ART_MAX];
int		RARTcount=0;
int		RARTenable=FALSE;
int 		LLOGenable=FALSE;

/*
**  See if a given newsgroup exists.
*/
GROUPENTRY *GRPfind(char *group)
{
    char		*p;
    unsigned int	j;
    GROUPENTRY		*gp;
    GRPHASH		*htp;
    char		c;

    /* SUPPRESS 6 *//* Over/underflow from plus expression */
    GRP_HASH(group, p, j);
    htp = GRP_BUCKET(j);
    for (c = *group, gp = htp->First; gp; gp = gp->Next) {
        if (c == GPNAME(gp)[0] && EQ(group, GPNAME(gp)))
            return gp;
    }
    return NULL;
}


STATIC void
GRPhash()
{
    register char		*p;
    register int		i;
    register GROUPENTRY		*gp;
    register unsigned int	j;
    register GRPHASH		*htp;

    /* Set up the default hash buckets. */
    GRPbuckets = GRPsize / GRP_SIZE;
    if (GRPbuckets == 0)
	GRPbuckets = 1;
    for (i = 0; i < GRP_SIZE; ++i)
	GRPtable[i].First = NULL;

#ifdef NOTDEF
    if (GRPtable[0].Groups)
	for (i = GRP_SIZE, htp = GRPtable; --i >= 0; htp++)
	    htp->Used = 0;
    else
	for (i = GRP_SIZE, htp = GRPtable; --i >= 0; htp++) {
	    htp->Size = GRPbuckets;
	    htp->Groups = NEW(GROUPENTRY*, htp->Size);
	    htp->Used = 0;
	}
#endif

    /* Now put all groups into the hash table. */
    for (i = GRPsize, gp = GRPentries; --i >= 0; gp++) {
	/* SUPPRESS 6 *//* Over/underflow from plus expression */
	GRP_HASH(GPNAME(gp), p, j);
	htp = GRP_BUCKET(j);
#ifdef NOTDEF
	if (htp->Used >= htp->Size) {
	    htp->Size += GRPbuckets;
	    RENEW(htp->Groups, GROUPENTRY*, htp->Size);
	}
	htp->Groups[htp->Used++] = gp;
#endif
	gp->Next = htp->First;
	htp->First = gp;
	
    }

    /* Note that we don't sort the buckets. */
}

long XRSize = -1;

char *
XReadInFile(const char *fileName, struct stat *pst)
{
    int fd;
    char *ptr = NULL;
    struct stat st;

    if (pst == NULL)
        pst = &st;

    fd = open(fileName, O_RDONLY);
    if (fd >= 0) {
        if (fstat(fd, pst) == 0) {
            ptr = (char *)mmap((caddr_t)NULL, pst->st_size + 1, PROT_READ, MAP_SHARED, fd, 0);
            if (ptr != (char *)-1) {
                XRSize = pst->st_size + 1;
                close(fd);
                /* syslog(L_ERROR, "%s mapped %d", ClientHost, pst->st_size + 1);*/
            } else {
                close(fd);
                return(ReadInFile(fileName, pst));
            }
        } else {
            close(fd);
        }
    }
    return(ptr);
}

void
XDISPOSE(char *active)
{
    if (XRSize >= 0) {
        munmap(active, XRSize);
        XRSize = -1;
    } else {
        DISPOSE(active);
    }
}

char *
GPNAME(GROUPENTRY *gp)
{
    const char *p = strchr(gp->Ptr, ' ');
    static char GPBuf[1024];

    GPBuf[0] = 0;
    if (p && p - gp->Ptr > 0 && p - gp->Ptr < sizeof(GPBuf) - 1) {
        memcpy(GPBuf, gp->Ptr, p - gp->Ptr);
        GPBuf[p - gp->Ptr] = 0;
    }
    return(GPBuf);
}

ARTNUM
GPHIGH(GROUPENTRY *gp)
{
    const char *p = strchr(gp->Ptr, ' ');
    long n = 0;
    if (p)
        n = strtol(p + 1, NULL, 10);
    return(n);
}

ARTNUM
GPLOW(GROUPENTRY *gp)
{
    const char *p = strchr(gp->Ptr, ' ');
    long n = 0;
    if (p) {
        if ((p = strchr(p + 1, ' ')) != NULL)
            n = strtol(p + 1, NULL, 10);
    }
    return(n);
}


char
GPFLAG(GROUPENTRY *gp)
{
    const char *p = strchr(gp->Ptr, ' ');
    char c = 'n';

    if (p) {
        if ((p = strchr(p + 1, ' ')) != NULL) {
            if ((p = strchr(p + 1, ' ')) != NULL) {
                c = p[1];
            }
        }
    }
    return(c);
}


char *
GPALIAS(GROUPENTRY *gp)
{
    const char *p = strchr(gp->Ptr, ' ');
    static char GPBuf[1024];

    GPBuf[0] = 0;

    if (p) {
        if ((p = strchr(p + 1, ' ')) != NULL) {
            if ((p = strchr(p + 1, ' ')) != NULL) {
                if (p[0] == NF_FLAG_ALIAS &&
                    p[1] != '\n' &&
                    strlen(p + 1) < sizeof(GPBuf) - 1
                ) {
                    strcpy(GPBuf, p + 1);
                }
            }
        }
    }
    return(GPBuf);
}

/*
**  Read the active file into memory, sort it, and set the number of
**  newsgroups read in.  Return TRUE if okay, FALSE on error.
*/
BOOL
GetGroupList()
{
    static char			*active;
    register char		*p;
    register char		*q;
    register GROUPENTRY		*gp;
    register int		i;

    /* If re-scanning, free previous groups. */
    if (active != NULL) {
	XDISPOSE(active);
	DISPOSE(GRPentries);
    }

    /* Get the new file. */
    active = XReadInFile(ACTIVE, (struct stat *)NULL);
    if (active == NULL) {
	syslog(L_ERROR, "%s cant read %s %m", ClientHost, ACTIVE);
	return FALSE;
    }

    /* Count lines. */
    for (p = active, i = 0; (p = strchr(p, '\n')) != NULL; p++, i++)
	continue;

    /* Fill in the group array. */
    GRPentries = NEW(GROUPENTRY, i);
    for (i = 0, gp = GRPentries, p = active; *p; i++, gp++, p = q + 1) {
	gp->Ptr = p;

#ifdef NOTDEF
	gp->Name = p;
#endif
	if ((p = strchr(p, ' ')) == NULL) {
	    syslog(L_ERROR, "%s internal no_space1 \"%.20s...\"",
		ClientHost, GPNAME(gp));
	    return FALSE;
	}
#ifdef NOTDEF
	*p++ = '\0';
#endif

	/* Get the high mark. */
	if ((q = strchr(p, ' ')) == NULL) {
	    syslog(L_ERROR, "%s internal no_space2 \"%.20s...\"",
		ClientHost, GPNAME(gp));
	    return FALSE;
	}
#ifdef NOTDEF
	*q++ = '\0';
	gp->High = atol(p);
#endif

	/* Get the low mark. */
	if ((p = strchr(q, ' ')) == NULL) {
	    syslog(L_ERROR, "%s internal no_space3 \"%.20s...\"",
		ClientHost, GPNAME(gp));
	    return FALSE;
	}
#ifdef NOTDEF
	*p++ = '\0';
	gp->Low = atol(q);
#endif

	/* Kill the newline. */
	if ((q = strchr(p, '\n')) == NULL) {
	    syslog(L_ERROR, "%s internal newline \"%.20s...\"",
		ClientHost, GPNAME(gp));
	    return FALSE;
	}
#ifdef NOTDEF
	*q = '\0';
	gp->Flag = *p;
	gp->Alias = gp->Flag == NF_FLAG_ALIAS ? p + 1 : NULL;
#endif
    }

    GRPsize = i;
    GRPhash();
    return TRUE;
}


/*
**  Sorting predicate to put newsgroup names into numeric order.
*/
STATIC int ARTcompare(CPOINTER p1, CPOINTER p2)
{
    return ((ARTLIST *)p1)->ArtNum - ((ARTLIST *)p2)->ArtNum;
}


/*
**  Fill in ARTnumbers with the numbers of the articles in the current
**  group.
*/
STATIC void GRPscandir(char *dir)
{
    static int		ARTarraysize;
    DIRENTRY	        *ep;
    DIR	                *dp;
    char	        *p;
    int	                i;
    char                *path;
    int                 fd;
    struct stat         sb;
    char                (*tmp)[][OVERINDEXPACKSIZE];
    int                 icount;
    OVERINDEX           index;

    ARTsize = 0;
    GRPcount++;

    path = NEW(char, strlen(innconf->pathoverview) + strlen(dir) +
				strlen(innconf->overviewname) + 32);
    sprintf(path, "%s/%s/%s.index", innconf->pathoverview, dir,
					innconf->overviewname);
    if ((fd = open(path, O_RDONLY)) >= 0) {
	DISPOSE(path);
	if (fstat(fd, &sb) < 0) {
	    syslog(L_ERROR, "%s cant fstat index %s %m", ClientHost, dir);
	    close(fd);
	    return;
	}
	icount = sb.st_size / OVERINDEXPACKSIZE;
	tmp = (char (*)[][OVERINDEXPACKSIZE])NEW(char, icount * OVERINDEXPACKSIZE);
	if (read(fd, tmp, icount * OVERINDEXPACKSIZE) != (icount * OVERINDEXPACKSIZE)) {
	    syslog(L_ERROR, "%s cant read index %s %m", ClientHost, dir);
	    close(fd);
	    return;
	}
	close(fd);
	if (OVERindex)
	    DISPOSE(OVERindex);
	OVERindex = tmp;
	OVERicount = icount;
	
	if (OVERicount > 0) {
	    if (ARTarraysize == 0) {
		ARTnumbers = NEW(ARTLIST, OVERicount);
	    } else {
		ARTnumbers = RENEW(ARTnumbers, ARTLIST, OVERicount);
	    }
	}
	ARTarraysize = OVERicount;
	for (i = 0; i < OVERicount; i++) {
	    UnpackOverIndex((*OVERindex)[i], &index);
	    ARTnumbers[ARTsize].ArtNum = index.artnum;
	    ARTnumbers[ARTsize].Token.cancelled = FALSE;
	    ARTnumbers[ARTsize].Token.type = TOKEN_EMPTY;
	    ARTnumbers[ARTsize].Tokenretrieved = FALSE;
	    ARTnumbers[ARTsize++].Index = &(*OVERindex)[i];
	}
	
    } else if (!innconf->storageapi) {
	DISPOSE(path);
	/* Go to the directory. */
	if (chdir(innconf->patharticles) < 0) {
	    syslog(L_FATAL, "%s cant cd %s %m", ClientHost,
					innconf->patharticles);
	    ExitWithStats(1);
	}

	if (ARTarraysize == 0) {
	    ARTarraysize = 1024;
	    ARTnumbers = NEW(ARTLIST, ARTarraysize);
	}

	/* The newsgroup directory might not exist; treat it as empty. */
	if (chdir(dir) < 0)
	    return;

	dp = opendir(".");
	if (dp == NULL) {
	    syslog(L_ERROR, "%s cant opendir %s %m", ClientHost, dir);
	    return;
	}
	
	while ((ep = readdir(dp)) != NULL) {
	    /* Get the numeric value of the filename, if it's all digits. */
	    for (p = ep->d_name, i = 0; *p; p++) {
		if (!CTYPE(isdigit, *p))
		    break;
		i = i * 10 + *p - '0';
	    }
	    if (*p || i == 0)
		continue;
	    
	    if (ARTsize + 1 >= ARTarraysize) {
		ARTarraysize += 1024;
		RENEW(ARTnumbers, ARTLIST, ARTarraysize);
	    }
	    
	    ARTnumbers[ARTsize].ArtNum = i;
	    ARTnumbers[ARTsize++].Index = NULL;
	}
	(void)closedir(dp);
    }

    ARTcache = NULL;
    qsort((POINTER)ARTnumbers, (SIZE_T)ARTsize, sizeof(ARTLIST), ARTcompare);
}


/*
**  Change to or list the specified newsgroup.  If invalid, stay in the old
**  group.
*/
FUNCTYPE CMDgroup(int ac, char *av[])
{
    static time_t	last_time;
    static char		NOSUCHGROUP[] = NNTP_NOSUCHGROUP;
    char	        *p;
    int	                i;
    time_t		now;
    char		*grplist[2];
    char		*group;
    char		buff[SPOOLNAMEBUFF];

    if (!PERMcanread) {
	Reply("%s\r\n", NOACCESS);
	return;
    }

    /* Parse arguments. */
    if (ac == 1) {
	if (GRPcount == 0) {
	    Printf("%d No group specified\r\n", NNTP_XGTITLE_BAD);
	    return;
	}
	(void)strcpy(buff, GRPlast);
	for (p = buff; *p; p++)
	    if (*p == '/')
		*p = '.';
	group = buff;
    }
    else
	group = av[1];
    if (GRPfind(group) == NULL) {
	Reply("%s %s\r\n", NOSUCHGROUP, group);
	return;
    }

    /* If permission is denied, pretend group doesn't exist. */
    if (PERMspecified) {
	grplist[0] = group;
	grplist[1] = NULL;
	if (!PERMmatch(PERMdefault, PERMlist, grplist)) {
	    Reply("%s %s\r\n", NOSUCHGROUP, group);
	    return;
	}
    }
    else if (!PERMdefault) {
	Reply("%s %s\r\n", NOSUCHGROUP, group);
	return;
    }

    /* Close out any existing article, report group stats. */
    ARTclose();
    ARTindex = 0;
    GRPreport();
    HIScheck();

    /* Make the group name a directory name. */
    if (buff != group)
	(void)strcpy(buff, group);
    for (p = buff; *p; p++)
	if (*p == '.')
	    *p = '/';

    /* If we haven't been in the group recently, rescan. */
    (void)time(&now);
    if (!EQ(buff, GRPlast) || now > last_time + NNRP_RESCAN_DELAY) {
	GRPscandir(buff);
	(void)strcpy(GRPlast, buff);
	last_time = now;
    }

    /* Close down any overview file. */
    OVERclose();

    /* Doing a "group" command? */
    if (caseEQ(av[0], "group")) {
	if (ARTsize == 0)
	    Reply("%d 0 0 0 %s\r\n", NNTP_GROUPOK_VAL, group);
	else
	    Reply("%d %d %ld %ld %s\r\n",
		NNTP_GROUPOK_VAL,
		ARTsize, ARTnumbers[0].ArtNum, ARTnumbers[ARTsize - 1].ArtNum, group);
    }
    else {
	/* Must be doing a "listgroup" command. */
	Reply("%d Article list follows\r\n", NNTP_GROUPOK_VAL);
	for (i = 0; i < ARTsize; i++)
	    Printf("%ld\r\n", ARTnumbers[i].ArtNum);
	Printf(".\r\n");
    }
}


/*
**  Report on the number of articles read in the group, and clear the count.
*/
void
GRPreport()
{
    register char	*p;
    register int	pp;
    char		buff[SPOOLNAMEBUFF];
    char		repbuff[1024], tmpbuff[80];

    if (GRPlast[0] && GRParticles != 0) {
	(void)strcpy(buff, GRPlast);
	for (p = buff; *p; p++)
	    if (*p == '/')
		*p = '.';
	syslog(L_NOTICE, "%s group %s %ld", ClientHost, buff, GRParticles);
	GRParticles = 0;
	repbuff[0]='\0';
	if (RARTenable && (RARTcount > 0)) {
		for (pp=0;pp<RARTcount; pp++) {
			sprintf(tmpbuff, "%ld", RARTtable[pp]);
			strcat(repbuff, tmpbuff);
			if (pp != RARTcount-1)
				strcat(repbuff, ",");
			RARTtable[pp]=-1;
		}
		syslog(L_NOTICE, "%s artcount (%s):%s:%s", ClientHost, Username, buff, repbuff);
		if (LLOGenable) {
			fprintf(locallog, "%s artcount (%s):%s:%s\n", ClientHost, Username, buff, repbuff);
			fflush(locallog);
		}
		RARTcount=0;
	}
			
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
	p = GRPlast;
    }
    else
	p = av[1];

    if (!PERMdefault) {
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
		if (!PERMmatch(PERMdefault, PERMlist, grplist))
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
