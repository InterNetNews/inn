/*  $Revision$
**
**  The newnews command.
*/
#include "config.h"
#include "clibrary.h"

#include "inn/innconf.h"
#include "nnrpd.h"
#include "ov.h"

#define GROUP_LIST_DELTA	10
#define OVFMT_UNINIT	-2
#define OVFMT_NOMSGID	-1
#define OVFMT_NOXREF	-1

static bool FindHeader(ARTHANDLE *art, const char **pp, const char **qp,
    const char* hdr, size_t hdrlen)
{
  const char *p, *p1, *q;
  bool Nocr = TRUE;

  if ((p = q = HeaderFindMem(art->data, art->len, hdr, hdrlen - 1)) == NULL)
    return false;
  for (p1 = NULL; p < art->data + art->len; p++) {
    if (p1 != NULL && *p1 == '\r' && *p == '\n') {
      Nocr = FALSE;
      break;
    }
    if (*p == '\n') {
      Nocr = TRUE;
      break;
    }
    p1 = p;
  }
  if (p >= art->data + art->len)
    return false;
  if (!Nocr)
    p = p1;

  *pp = p;
  *qp = q;
  return true;
}

/*
**  get Xref header
*/
static char *GetXref(ARTHANDLE *art) {
  const char	*p, *q;
  static char	buff[BIG_BUFFER];

  if (!FindHeader(art, &p, &q, "xref", sizeof("xref")))
    return NULL;
  if (p - q > BIG_BUFFER - 1)
    return NULL;
  memcpy(buff, q, p - q);
  buff[p - q] = '\0';
  return buff;
}

/*
**  get Message-ID header
*/
static char *GetMsgid(ARTHANDLE *art) {
  const char		*p, *q;
  static char	buff[BIG_BUFFER];

  if (!FindHeader(art, &p, &q, "message-id", sizeof("message-id")))
    return NULL;
  if (p - q > BIG_BUFFER - 1)
    return NULL;
  memcpy(buff, q, p - q);
  buff[p - q] = '\0';
  return buff;
}

/*
**  Split newsgroup list into array of newsgroups.  Return static pointer,
**  or NULL if there are no newsgroup.
*/
static char **GetGroups(char *p) {
  static int	size;
  static char	**list;
  int		i;
  char		*q;
  static char	*Xrefbuf = NULL;
  char		*Xref = p;

  if (size == 0) {
    size = GROUP_LIST_DELTA;
    list = NEW(char*, size + 1);
  }
  if ((Xref = strstr(Xref, "Xref:")) != NULL) {
    if ((Xref = strchr(Xref, ' ')) == NULL)
      return NULL;
  } else
    Xref = p;
  for (Xref++; *Xref == ' '; Xref++);
  if ((Xref = strchr(Xref, ' ')) == NULL)
    return NULL;
  for (Xref++; *Xref == ' '; Xref++);
  if (!Xrefbuf)
    Xrefbuf = NEW(char, BIG_BUFFER);
  strcpy(Xrefbuf, Xref);
  if ((q = strchr(Xrefbuf, '\t')) != NULL)
    *q = '\0';
  p = Xrefbuf;

  for (i = 0 ; ;i++) {
    while (ISWHITE(*p))
      p++;
    if (*p == '\0' || *p == '\n')
      break;

    if (i >= size - 1) {
      size += GROUP_LIST_DELTA;
      RENEW(list, char *, size + 1);
    }
    for (list[i] = p; *p && *p != '\n' && !ISWHITE(*p); p++) {
      if (*p == '/' || *p == ':')
	*p = '\0';
    }
    if (*p) *p++ = '\0';
  }
  list[i] = NULL;
  return i ? list : NULL;
}

static bool HaveSeen(bool AllGroups, char *group, char **groups, char **xrefs) {
  char *list[2];

  list[1] = NULL;
  for ( ; *xrefs; xrefs++) {
    list[0] = *xrefs;
    if ((!AllGroups && PERMmatch(groups, list)) && (!PERMspecified || (PERMspecified && PERMmatch(PERMreadlist, list)))) {
      if (!strcmp(*xrefs, group))
	return FALSE;
      else
	return TRUE;
    }
  }
  return FALSE;
}

/*
**  NEWNEWS newsgroups date time ["GMT"]
**  Return the Message-ID of any articles after the specified date
*/
void CMDnewnews(int ac, char *av[]) {
  static char	**groups;
  char		*group;
  char		*p, *q;
  char          *path;
  bool		AllGroups;
  char		**xrefs;
  char		line[BIG_BUFFER];
  time_t	date;
  TOKEN		token;
  ARTHANDLE	*art = NULL;
  QIOSTATE	*qp;
  char		*grplist[2];
  int		count;
  void		*handle;
  time_t	arrived;
  char		*data;
  int		i, len;
  static int	Msgid = OVFMT_UNINIT;
  static int	Xref = OVFMT_UNINIT;
  bool          local;

  if (!PERMaccessconf->allownewnews) {
    Reply("%d NEWNEWS command disabled by administrator\r\n", NNTP_ACCESS_VAL);
    return;
  }

  if (!PERMcanread) {
    Reply("%s\r\n", NNTP_ACCESS);
    return;
  }

  /* Make other processes happier if someone uses NEWNEWS */
  if (innconf->nicenewnews > 0)
    nice(innconf->nicenewnews);

  snprintf(line, sizeof(line), "%s %s %s %s %s", av[1], av[2], av[3],
    (ac >= 5 && (*av[4] == 'G' || *av[4] == 'U')) ? "GMT" : "local",
    (ac >= 5 && *av[ac - 1] == '<') ? av[ac - 1] : "none");
  syslog(L_NOTICE, "%s newnews %s", ClientHost, line);

  TMRstart(TMR_NEWNEWS);
  /* Optimization in case client asks for !* (no groups) */
  if (EQ(av[1], "!*")) {
    Reply("%s\r\n", NNTP_NEWNEWSOK);
    Printf(".\r\n");
    TMRstop(TMR_NEWNEWS);
    return;
  }

  /* Parse the newsgroups. */
  AllGroups = EQ(av[1], "*");
  if (!AllGroups && !NGgetlist(&groups, av[1])) {
    Reply("%d Bad newsgroup specifier %s\r\n", NNTP_SYNTAX_VAL, av[1]);
    TMRstop(TMR_NEWNEWS);
    return;
  }

  /* Parse the date. */
  local = !(ac > 4 && caseEQ(av[4], "GMT"));
  date = parsedate_nntp(av[2], av[3], local);
  if (date == (time_t) -1) {
    Reply("%d Bad date\r\n", NNTP_SYNTAX_VAL);
    TMRstop(TMR_NEWNEWS);
    return;
  }

  path = concatpath(innconf->pathdb, _PATH_ACTIVE);
  qp = QIOopen(path);
  if (qp == NULL) {
    if (errno == ENOENT) {
      Reply("%d Can't open active\r\n", NNTP_TEMPERR_VAL);
    } else {
      syslog(L_ERROR, "%s cant fopen %s %m", ClientHost, path);
      Reply("%d Can't open active\r\n", NNTP_TEMPERR_VAL);
    }
    free(path);
    TMRstop(TMR_NEWNEWS);
    return;
  }
  free(path);

  Reply("%s\r\n", NNTP_NEWNEWSOK);

  if (Msgid == OVFMT_UNINIT) {
    for (Msgid = OVFMT_NOMSGID, i = 0; i < ARTfieldsize; i++) {
      if (caseEQ(ARTfields[i].Header, "Message-ID")) {
	Msgid = i;
	break;
      }
    }
    for (Xref = OVFMT_NOXREF, i = 0; i < ARTfieldsize; i++) {
      if (caseEQ(ARTfields[i].Header, "Xref")) {
	Xref = i;
	break;
      }
    }
  }

  while ((p = QIOread(qp)) != NULL) {
    for (q = p; *q != '\0'; q++) {
      if (*q == ' ' || *q == '\t') {
	*q = '\0';
	break;
      }
    }
    grplist[0] = group = p;
    grplist[1] = NULL;
    if (PERMspecified && !PERMmatch(PERMreadlist, grplist))
      continue;
    if (!AllGroups && !PERMmatch(groups, grplist))
      continue;
    if (!OVgroupstats(group, &ARTlow, &ARThigh, &count, NULL))
      continue;
    if ((handle = OVopensearch(group, ARTlow, ARThigh)) != NULL) {
      while (OVsearch(handle, NULL, &data, &len, &token, &arrived)) {
	if (len == 0 || date > arrived)
	  continue;
	if (Msgid == OVFMT_NOMSGID || Xref == OVFMT_NOXREF) {
	  if ((art = SMretrieve(token, RETR_HEAD)) == NULL)
	    continue;
	  if (Msgid != OVFMT_NOMSGID && Xref != OVFMT_NOXREF)
	    SMfreearticle(art);
	} else if (PERMaccessconf->nnrpdcheckart && !ARTinstorebytoken(token))
	  continue;
	if (Xref != OVFMT_NOXREF) {
	  if ((p = OVERGetHeader(data, len, Xref)) == NULL) {
	    if (Msgid == OVFMT_NOMSGID)
	      SMfreearticle(art);
	    continue;
	  }
	} else {
	  if ((p = GetXref(art)) == NULL) {
	    SMfreearticle(art);
	    continue;
	  }
	}
	if ((xrefs = GetGroups(p)) == NULL) {
	  if (Msgid == OVFMT_NOMSGID)
	    SMfreearticle(art);
	  continue;
	}
	if (HaveSeen(AllGroups, group, groups, xrefs)) {
	  if (Msgid == OVFMT_NOMSGID)
	    SMfreearticle(art);
	  continue;
	}
	if (Msgid != OVFMT_NOMSGID) {
	  if ((p = OVERGetHeader(data, len, Msgid)) == NULL) {
	    continue;
	  }
	} else {
	  if ((p = GetMsgid(art)) == NULL) {
	    SMfreearticle(art);
	    continue;
	  }
	  SMfreearticle(art);
	}
	if (innconf->nfsreader &&
	    !HISlookup(History, p, NULL, NULL, NULL, NULL))
	  continue;
	Printf("%s\r\n", p);
      }
      OVclosesearch(handle);
    }
    continue;
  }
  (void)QIOclose(qp);
  Printf(".\r\n");
  TMRstop(TMR_NEWNEWS);
}
