/*  $Revision$
**
**  The newnews command.
*/
#include "config.h"
#include "clibrary.h"
#include <netinet/in.h>

#include "nnrpd.h"
#include "ov.h"

#define GROUP_LIST_DELTA	10
#define GET_DATE(p, line)	\
	((p = strchr(line, HIS_FIELDSEP)) == NULL ? 0 : atol(++p))
#define OVFMT_UNINIT	-2
#define OVFMT_NOMSGID	-1
#define OVFMT_NOXREF	-1

/*
**  see if its distribution is in the list.
*/
STATIC BOOL DistMatches(ARTHANDLE *art, char **distribs) {
  char  **dp;
  char	*p, *p1;
  char	*q;
  BOOL	Nocr;

  if ((p = q = (char *)HeaderFindMem(art->data, art->len, "distribution", sizeof("distribution")-1)) == NULL)
    return FALSE;
  for (p1 = NULL; p < art->data + art->len; p++) {
    if (p1 != (char *)NULL && *p1 == '\r' && *p == '\n') {
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
    return FALSE;
  if (!Nocr)
    p = p1;
  for (dp = distribs; *dp; dp++)
    if (caseEQn(q, *dp, p - q))
      return TRUE;
  return FALSE;
}

/*
**  get Xref header
*/
STATIC char *GetXref(ARTHANDLE *art) {
  char		*p, *p1;
  char		*q;
  static char	buff[BIG_BUFFER];
  BOOL		Nocr;

  if ((p = q = (char *)HeaderFindMem(art->data, art->len, "xref", sizeof("xref")-1)) == NULL)
    return NULL;
  for (p1 = NULL; p < art->data + art->len; p++) {
    if (p1 != (char *)NULL && *p1 == '\r' && *p == '\n') {
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
    return NULL;
  if (!Nocr)
    p = p1;
  if (p - q > BIG_BUFFER - 1)
    return NULL;
  memcpy(buff, q, p - q);
  buff[p - q] = '\0';
  return buff;
}

/*
**  get Message-ID header
*/
STATIC char *GetMsgid(ARTHANDLE *art) {
  char		*p, *p1;
  char		*q;
  static char	buff[BIG_BUFFER];
  BOOL		Nocr;

  if ((p = q = (char *)HeaderFindMem(art->data, art->len, "message-id", sizeof("message-id")-1)) == NULL)
    return NULL;
  for (p1 = NULL; p < art->data + art->len; p++) {
    if (p1 != (char *)NULL && *p1 == '\r' && *p == '\n') {
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
    return NULL;
  if (!Nocr)
    p = p1;
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
STATIC char **GetGroups(char *p) {
  static int	size;
  static char	**list;
  int		i, len;
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
  len = strlen(p);
  memcpy(Xrefbuf, Xref, len - (p - Xref));
  Xrefbuf[len - (p - Xref)] = '\0';
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

STATIC BOOL HaveSeen(BOOL AllGroups, char *group, char **groups, char **xrefs) {
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
**  NEWNEWS newsgroups date time ["GMT"|"UTC"] [<distributions>]
**  Return the Message-ID of any articles after the specified date,
**  and within the specified distributions.
*/
FUNCTYPE CMDnewnews(int ac, char *av[]) {
  static char	**groups;
  char		*group;
  char		*p, *q;
  BOOL		AllDists;
  BOOL		AllGroups;
  char		**distribs;
  char		**xrefs;
  char		line[BIG_BUFFER];
  long		date;
  TOKEN		token;
  ARTHANDLE	*art;
  QIOSTATE	*qp;
  char		*grplist[2];
  int		count;
  void		*handle;
  time_t	arrived;
  char		*data;
  int		i, len;
  static int	Msgid = OVFMT_UNINIT;
  static int	Xref = OVFMT_UNINIT;

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

  (void)sprintf(line, "%s %s %s %s %s", av[1], av[2], av[3],
    (ac >= 5 && (*av[4] == 'G' || *av[4] == 'U')) ? "GMT" : "local",
    (ac >= 5 && *av[ac - 1] == '<') ? av[ac - 1] : "none");
  syslog(L_NOTICE, "%s newnews %s", ClientHost, line);

  /* Optimization in case client asks for !* (no groups) */
  if (EQ(av[1], "!*")) {
    Reply("%s\r\n", NNTP_NEWNEWSOK);
    Printf(".\r\n");
    return;
  }

  /* Parse the newsgroups. */
  AllGroups = EQ(av[1], "*");
  if (!AllGroups && !NGgetlist(&groups, av[1])) {
    Reply("%d Bad newsgroup specifier %s\r\n", NNTP_SYNTAX_VAL, av[1]);
    return;
  }

  /* Parse the date. */
  date = NNTPtoGMT(av[2], av[3]);
  if (date < 0) {
    Reply("%d Bad date\r\n", NNTP_SYNTAX_VAL);
    return;
  }
  ac -= 4;
  av += 4;
  if (ac > 0 && (caseEQ(*av, "GMT") || caseEQ(*av, "UTC"))) {
    ac--;
    av++;
  } else
    date = LOCALtoGMT(date);

  /* Parse the distributions. */
  if (ac == 0)
    AllDists = TRUE;
  else {
    if (!ParseDistlist(&distribs, *av)) {
      Reply("%d Bad distribution %s\r\n", NNTP_SYNTAX_VAL, *av);
      return;
    }
    AllDists = FALSE;
  }

  if ((qp = QIOopen(cpcatpath(innconf->pathdb, _PATH_ACTIVE))) == NULL) {
    if (errno == ENOENT) {
      Reply("%d Can't open active\r\n", NNTP_TEMPERR_VAL);
    } else {
      syslog(L_ERROR, "%s cant fopen %s %m", ClientHost, cpcatpath(innconf->pathdb, _PATH_ACTIVE));
      Reply("%d Can't open active\r\n", NNTP_TEMPERR_VAL);
    }
    return;
  }

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
	if (!AllDists || Msgid == OVFMT_NOMSGID || Xref == OVFMT_NOXREF) {
	  if ((art = SMretrieve(token, RETR_HEAD)) == NULL)
	    continue;
	  if (!AllDists && !DistMatches(art, distribs)) {
	    SMfreearticle(art);
	    continue;
	  }
	  if (Msgid != OVFMT_NOMSGID && Xref != OVFMT_NOXREF)
	    SMfreearticle(art);
	}
	if (Xref != OVFMT_NOXREF) {
	  if ((p = OVERGetHeader(data, Xref)) == NULL) {
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
	  if ((p = OVERGetHeader(data, Msgid)) == NULL) {
	    continue;
	  }
	} else {
	  if ((p = GetMsgid(art)) == NULL) {
	    SMfreearticle(art);
	    continue;
	  }
	  SMfreearticle(art);
	}
	Printf("%s\r\n", p);
      }
      OVclosesearch(handle);
    }
    continue;
  }
  (void)QIOclose(qp);
  Printf(".\r\n");
}
