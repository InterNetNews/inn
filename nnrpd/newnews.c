/*  $Revision$
**
**  The newnews command.
*/
#include "config.h"
#include "clibrary.h"

#include "inn/innconf.h"
#include "inn/messages.h"
#include "inn/wire.h"
#include "nnrpd.h"
#include "inn/ov.h"
#include "cache.h"

#define GROUP_LIST_DELTA	10

static bool
FindHeader(ARTHANDLE *art, const char **pp, const char **qp,
           const char* hdr, size_t hdrlen UNUSED)
{
  const char *p, *p1, *q;
  bool Nocr = true;

  p = wire_findheader(art->data, art->len, hdr);
  if (p == NULL)
    return false;
  q = p;
  for (p1 = NULL; p < art->data + art->len; p++) {
    if (p1 != NULL && *p1 == '\r' && *p == '\n') {
      Nocr = false;
      break;
    }
    if (*p == '\n') {
      Nocr = true;
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
static char *
GetXref(ARTHANDLE *art)
{
  const char	*p, *q;

  if (!FindHeader(art, &p, &q, "xref", sizeof("xref")))
    return NULL;
  return xstrndup(q, p - q);
}

/*
**  Split newsgroup list into array of newsgroups.  Return static pointer,
**  or NULL if there are no newsgroup.
*/
static char **
GetGroups(char *p)
{
  static int	size;
  static char	**list;
  int		i;
  char		*q;
  static char	*Xrefbuf = NULL;
  char		*Xref = p;

  if (size == 0) {
    size = GROUP_LIST_DELTA;
    list = xmalloc((size + 1) * sizeof(char *));
  }
  Xref = p;
  for (Xref++; *Xref == ' '; Xref++);
  if ((Xref = strchr(Xref, ' ')) == NULL)
    return NULL;
  for (Xref++; *Xref == ' '; Xref++);
  if (!Xrefbuf)
    Xrefbuf = xmalloc(BIG_BUFFER);
  strlcpy(Xrefbuf, Xref, BIG_BUFFER);
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
      list = xrealloc(list, (size + 1) * sizeof(char *));
    }
    for (list[i] = p; *p && *p != '\n' && !ISWHITE(*p); p++) {
      if (*p == ':')
	*p = '\0';
    }
    if (*p) *p++ = '\0';
  }
  list[i] = NULL;
  return i ? list : NULL;
}

static bool
HaveSeen(bool AllGroups, char *group, char **groups, char **xrefs)
{
  char *list[2];

  list[1] = NULL;
  for ( ; *xrefs; xrefs++) {
    list[0] = *xrefs;
    if ((!AllGroups && PERMmatch(groups, list)) && (!PERMspecified || (PERMspecified && PERMmatch(PERMreadlist, list)))) {
      if (!strcmp(*xrefs, group))
	return false;
      else
	return true;
    }
  }
  return false;
}

static char **groups;

static void
process_newnews(char *group, bool AllGroups, time_t date)
{
    int low, high;
    char **xrefs;
    int count;
    void *handle;
    char *p;
    time_t arrived;
    ARTHANDLE *art = NULL;
    TOKEN token;
    char *data;
    int len;
    char *grplist[2];
    time_t now;

    grplist[0] = group;
    grplist[1] = NULL;
    if (PERMspecified && !PERMmatch(PERMreadlist, grplist))
	return;
    if (!AllGroups && !PERMmatch(groups, grplist))
	return;
    if (!OVgroupstats(group, &low, &high, &count, NULL))
	return;
    ARTlow = low;
    ARThigh = high;
    if ((handle = OVopensearch(group, ARTlow, ARThigh)) != NULL) {
	ARTNUM artnum;
	unsigned long artcount = 0;
	struct cvector *vector = NULL;

	if (innconf->nfsreader) {
	    time(&now);
	    /* move the start time back nfsreaderdelay seconds */
	    if (date >= innconf->nfsreaderdelay)
		date -= innconf->nfsreaderdelay;
	}
	while (OVsearch(handle, &artnum, &data, &len, &token, &arrived)) {
	    if (innconf->nfsreader && arrived + innconf->nfsreaderdelay > now)
		continue;
	    if (len == 0 || date > arrived)
		continue;

	    vector = overview_split(data, len, NULL, vector);
	    if (overhdr_xref == -1) {
		if ((art = SMretrieve(token, RETR_HEAD)) == NULL)
		    continue;
		p = GetXref(art);
		SMfreearticle(art);
	    } else {
		if (PERMaccessconf->nnrpdcheckart && 
		    !ARTinstorebytoken(token))
		    continue;
		/* We only care about the newsgroup list here, virtual
		 * hosting isn't relevant */
		p = overview_getheader(vector, overhdr_xref, OVextra);
	    }
	    if (p == NULL)
		continue;
	    xrefs = GetGroups(p);
	    free(p);
	    if (xrefs == NULL)
		continue;
	    if (HaveSeen(AllGroups, group, groups, xrefs))
		continue;
	    p = overview_getheader(vector, OVERVIEW_MESSAGE_ID, OVextra);
	    if (p == NULL)
		continue;

	    ++artcount;
	    cache_add(HashMessageID(p), token);
	    Printf("%s\r\n", p);
	    free(p);
	}
	OVclosesearch(handle);
	notice("%s newnews %s %lu", Client.host, group, artcount);
	if (vector)
	    cvector_free(vector);
    }
}

/*
**  NEWNEWS wildmat date time ["GMT"]
**  Return the Message-ID of any articles after the specified date
*/
void
CMDnewnews(int ac, char *av[])
{
  char		*p, *q;
  char          *path;
  bool		AllGroups;
  char		line[BIG_BUFFER];
  time_t	date;
  QIOSTATE	*qp;
  int		i;
  bool          local;

  if (!PERMaccessconf->allownewnews) {
      Reply("%d NEWNEWS command disabled by administrator\r\n", NNTP_ERR_ACCESS);
      return;
  }

  if (!PERMcanread) {
      Reply("%s\r\n", NNTP_ACCESS);
      return;
  }

  /* Make other processes happier if someone uses NEWNEWS */
  if (innconf->nicenewnews > 0) {
      nice(innconf->nicenewnews);
      innconf->nicenewnews = 0;
  }

  snprintf(line, sizeof(line), "%s %s %s %s", av[1], av[2], av[3],
	   (ac >= 5 && (*av[4] == 'G' || *av[4] == 'U')) ? "GMT" : "local");
  notice("%s newnews %s", Client.host, line);

  TMRstart(TMR_NEWNEWS);
  /* Optimization in case client asks for !* (no groups) */
  if (strcmp(av[1], "!*") == 0) {
      Reply("%s\r\n", NNTP_NEWNEWSOK);
      Printf(".\r\n");
      TMRstop(TMR_NEWNEWS);
      return;
  }

  /* Parse the newsgroups. */
  AllGroups = (strcmp(av[1], "*") == 0);
  if (!AllGroups && !NGgetlist(&groups, av[1])) {
      Reply("%d Bad newsgroup specifier %s\r\n", NNTP_ERR_SYNTAX, av[1]);
      TMRstop(TMR_NEWNEWS);
      return;
  }

  /* Parse the date. */
  local = !(ac > 4 && strcasecmp(av[4], "GMT") == 0);
  date = parsedate_nntp(av[2], av[3], local);
  if (date == (time_t) -1) {
      Reply("%d Bad date\r\n", NNTP_ERR_SYNTAX);
      TMRstop(TMR_NEWNEWS);
      return;
  }

  if (strcspn(av[1], "\\!*[?]") == strlen(av[1])) {
      /* optimise case - don't need to scan the active file pattern
       * matching */
      Reply("%s\r\n", NNTP_NEWNEWSOK);
      for (i = 0; groups[i]; ++i) {
	  process_newnews(groups[i], AllGroups, date);
      }
  } else {
      path = concatpath(innconf->pathdb, INN_PATH_ACTIVE);
      qp = QIOopen(path);
      if (qp == NULL) {
	  if (errno == ENOENT) {
	      Reply("%d Can't open active\r\n", NNTP_ERR_UNAVAILABLE);
	  } else {
	      syswarn("%s cant fopen %s", Client.host, path);
	      Reply("%d Can't open active\r\n", NNTP_ERR_UNAVAILABLE);
	  }
	  free(path);
	  TMRstop(TMR_NEWNEWS);
	  return;
      }
      free(path);

      Reply("%s\r\n", NNTP_NEWNEWSOK);

      while ((p = QIOread(qp)) != NULL) {
	  for (q = p; *q != '\0'; q++) {
	      if (*q == ' ' || *q == '\t') {
		  *q = '\0';
		  break;
	      }
	  }
	  process_newnews(p, AllGroups, date);
      }
      QIOclose(qp);
  }
  Printf(".\r\n");
  TMRstop(TMR_NEWNEWS);
}
