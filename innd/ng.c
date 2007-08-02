/*  $Id$
**
**  Routine for the in-core data structures for the active and newsfeeds
**  files.
*/

#include "config.h"
#include "clibrary.h"
#include <dirent.h>

#include "inn/innconf.h"
#include "innd.h"
#include "inn/ov.h"


/*
**  Hash function taken from Chris Torek's hash package posted to
**  comp.lang.c on 18-Oct-90 in <27038@mimsy.umd.edu>.  Thanks, Chris.
*/
#define NGH_HASH(Name, p, j)	\
	for (p = Name, j = 0; *p; ) j = (j << 5) + j + *p++


/*
**  Size of hash table.   Change NGH_BUCKET if not a power of two.
*/
#define NGH_SIZE	2048
#define NGH_BUCKET(j)	&NGHtable[j & (NGH_SIZE - 1)]


/*
**  Newsgroup hash entry, which is really a hash bucket -- pointers
**  to all the groups with this hash code.
*/
typedef struct _NGHASH {
    int			Size;
    int			Used;
    NEWSGROUP		**Groups;
} NGHASH;


static struct buffer	NGnames;
static NGHASH		NGHtable[NGH_SIZE];
static int		NGHbuckets;
static int		NGHcount;



/*
**  Sorting predicate for qsort call in NGparsefile.  Put newsgroups in
**  rough order of their activity.  Will be better if we write a "counts"
**  file sometime.
*/
static int
NGcompare(const void *p1, const void *p2)
{
    const NEWSGROUP *const *n1 = p1;
    const NEWSGROUP *const *n2 = p2;

    return n1[0]->Last - n2[0]->Last;
}


/*
**  Parse a single line from the active file, filling in ngp.  Be careful
**  not to write NUL's into the in-core copy, since we're either mmap(2)'d,
**  or we want to just blat it out to disk later.
*/
static bool
NGparseentry(NEWSGROUP *ngp, const char *p, char *end)
{
    char		*q;
    unsigned int	j;
    NGHASH		*htp;
    NEWSGROUP		**ngpp;
    int			i;
    ARTNUM		lo;

    if ((q = strchr(p, ' ')) == NULL)
	return false;
    i = q - p;

    ngp->NameLength = i;
    ngp->Name = &NGnames.data[NGnames.used];
    strncpy(ngp->Name, p, i);
    ngp->Name[i] = '\0';
    NGnames.used += i + 1;

    ngp->LastString = ++q;
    if ((q = strchr(q, ' ')) == NULL || q > end)
	return false;
    ngp->Lastwidth = q - ngp->LastString;
    if ((q = strchr(q, ' ')) == NULL || q > end)
	return false;
    lo = (ARTNUM)atol(q + 1);
    if ((q = strchr(q + 1, ' ')) == NULL || q > end)
	return false;
    ngp->Rest = ++q;
    /* We count on atoi() to stop at the space after the digits! */
    ngp->Last = atol(ngp->LastString);
    ngp->nSites = 0;
    ngp->Sites = xmalloc(NGHcount * sizeof(int));
    ngp->nPoison = 0;
    ngp->Poison = xmalloc(NGHcount * sizeof(int));
    ngp->Alias = NULL;

    /* Find the right bucket for the group, make sure there is room. */
    NGH_HASH(ngp->Name, p, j);
    htp = NGH_BUCKET(j);
    for (p = ngp->Name, ngpp = htp->Groups, i = htp->Used; --i >= 0; ngpp++)
	if (*p == ngpp[0]->Name[0] && strcmp(p, ngpp[0]->Name) == 0) {
	    syslog(L_ERROR, "%s duplicate_group %s", LogName, p);
	    return false;
	}
    if (htp->Used >= htp->Size) {
	htp->Size += NGHbuckets;
        htp->Groups = xrealloc(htp->Groups, htp->Size * sizeof(NEWSGROUP *));
    }
    htp->Groups[htp->Used++] = ngp;

    if (innconf->enableoverview && !OVgroupadd(ngp->Name, lo, ngp->Last, ngp->Rest))
	return false;

    return true;
}


/*
**  Parse the active file, building the initial Groups global.
*/
void
NGparsefile(void)
{
    char	*p;
    char	*q;
    int		i;
    bool	SawMe;
    NEWSGROUP	*ngp;
    NGHASH	*htp;
    char	**strings;
    char	*active;
    char	*end;

    /* If re-reading, remove anything we might have had. */
    NGclose();

    /* Get active file and space for group entries. */
    active = ICDreadactive(&end);
    for (p = active, i = 0; p < end; p++)
	if (*p == '\n') i++;
    if ((nGroups = i) == 0) {
	syslog(L_FATAL, "%s empty active file", LogName);
	exit(1);
    }
    Groups = xmalloc(nGroups * sizeof(NEWSGROUP));
    GroupPointers = xmalloc(nGroups * sizeof(NEWSGROUP *));

    /* Get space to hold copies of the names.  This might take more space
     * than individually allocating each element, but it is definitely easier
     * on the system. */
    i = end - active;
    NGnames.size = i;
    NGnames.data = xmalloc(NGnames.size + 1);
    NGnames.used = 0;

    /* Set up the default hash buckets. */
    NGHbuckets = nGroups / NGH_SIZE;
    if (NGHbuckets == 0)
	NGHbuckets = 1;
    if (NGHtable[0].Groups)
	for (i = NGH_SIZE, htp = NGHtable; --i >= 0; htp++)
	    htp->Used = 0;
    else
	for (i = NGH_SIZE, htp = NGHtable; --i >= 0; htp++) {
	    htp->Size = NGHbuckets;
	    htp->Groups = xmalloc(htp->Size * sizeof(NEWSGROUP *));
	    htp->Used = 0;
	}

    /* Count the number of sites. */
    SawMe = false;
    for (strings = SITEreadfile(true), i = 0; (p = strings[i]) != NULL; i++)
	if (*p == 'M' && *++p == 'E' && *++p == ':')
	    SawMe = true;
    if (i == 0 || (i == 1 && SawMe)) {
	syslog(L_ERROR, "%s bad_newsfeeds no feeding sites", LogName);
	NGHcount = 1;
    }
    else
	NGHcount = i;

    /* Loop over all lines in the active file, filling in the fields of
     * the Groups array. */
    for (p = active, ngp = Groups, i = nGroups; --i >= 0; ngp++, p = q + 1) {
	ngp->Start = p - active;
	if ((q = strchr(p, '\n')) == NULL || !NGparseentry(ngp, p, q)) {
	    syslog(L_FATAL, "%s bad_active %s...", LogName, MaxLength(p, q));
	    exit(1);
	}
    }

    /* Sort each bucket. */
    for (i = NGH_SIZE, htp = NGHtable; --i >= 0; htp++)
	if (htp->Used > 1)
	    qsort(htp->Groups, htp->Used, sizeof htp->Groups[0], NGcompare);

    /* Chase down any alias flags. */
    for (ngp = Groups, i = nGroups; --i >= 0; ngp++)
	if (ngp->Rest[0] == NF_FLAG_ALIAS) {
	    ngp->Alias = ngp;
	    if ((p = strchr(ngp->Alias->Rest, '\n')) != NULL)
		*p = '\0';
	    ngp->Alias = NGfind(&ngp->Alias->Rest[1]);
	    if (p)
		*p = '\n';
	    if (ngp->Alias != NULL && ngp->Alias->Rest[0] == NF_FLAG_ALIAS)
		syslog(L_NOTICE, "%s alias_error %s too many levels",
		    LogName, ngp->Name);
	}
}

/*
** Free allocated memory
*/
void
NGclose(void)
{
    int		i;
    NEWSGROUP	*ngp;
    NGHASH	*htp;

    if (Groups) {
	for (i = nGroups, ngp = Groups; --i >= 0; ngp++) {
	    free(ngp->Sites);
	    free(ngp->Poison);
	}
	free(Groups);
	Groups = NULL;
	free(GroupPointers);
	free(NGnames.data);
    }

    for (i = NGH_SIZE, htp = NGHtable; --i >= 0; htp++) {
      htp->Size = NGHbuckets;
      if (htp->Groups) {
	free(htp->Groups);
	htp->Used = 0;
	htp->Groups = NULL;
      }
    }
}

/*
**  Hash a newsgroup and see if we get it.
*/
NEWSGROUP *
NGfind(const char *Name)
{
    const char		*p;
    int			i;
    unsigned int	j;
    NEWSGROUP		**ngp;
    char		c;
    NGHASH		*htp;

    NGH_HASH(Name, p, j);
    htp = NGH_BUCKET(j);
    for (c = *Name, ngp = htp->Groups, i = htp->Used; --i >= 0; ngp++)
	if (c == ngp[0]->Name[0] && strcmp(Name, ngp[0]->Name) == 0)
	    return ngp[0];
    return NULL;
}


/*
**  Split a newsgroups header line into the groups we get.  Return the
**  number of newsgroups.  ' ' and '\t' are dropped when copying.
*/
int
NGsplit(char *p, int size, LISTBUFFER *list)
{
  char		**gp, *q;
  int		i;

  /* setup buffer */
  SetupListBuffer(size, list);

  /* loop over and copy */
  for (i = 0, q = list->Data, gp = list->List ; *p ; p++, *q++ = '\0') {
    /* skip leading separators and white spaces. */
    for (; *p && (NG_ISSEP(*p) || ISWHITE(*p)) ; p++)
      continue;
    if (*p == '\0')
      break;

    if (i == list->ListLength) {
      list->ListLength += DEFAULTNGBOXSIZE;
      list->List = xrealloc(list->List, list->ListLength * sizeof(char *));
      gp = &list->List[i];
    }
    /* mark the start of the newsgroup, move to the end of it while copying */
    for (*gp++ = q, i++ ; *p && !NG_ISSEP(*p) && !ISWHITE(*p) ;) {
      if (*p == ':')
	/* reject if ':' is included */
	return 0;
      *q++ = *p++;
      continue;
    }
    if (*p == '\0')
      break;
  }
  *q = '\0';
  if (i == list->ListLength) {
    list->ListLength += DEFAULTNGBOXSIZE;
    list->List = xrealloc(list->List, list->ListLength * sizeof(char *));
    gp = &list->List[i];
  }
  *gp = NULL;
  return i;
}


/*
**  Renumber a group.
*/
static char		NORENUMBER[] = "%s cant renumber %s %s too wide";
static char		RENUMBER[] = "%s renumber %s %s from %ld to %ld";

bool
NGrenumber(NEWSGROUP *ngp)
{
    int			low, high, count,flag;
    char	        *f2;
    char		*f3;
    char		*f4;
    char		*start;
    long		lomark, himark;
    long		l;
    char		*dummy;

    if (!innconf->enableoverview) return true; /* can't do anything w/o overview */

    /* Get a valid offset into the active file. */
    if (ICDneedsetup) {
	syslog(L_ERROR, "%s unsynched must reload before renumber", LogName);
	return false;
    }
    start = ICDreadactive(&dummy) + ngp->Start;

    /* Check the file format. */
    if ((f2 = strchr(start, ' ')) == NULL
     || (f3 = strchr(++f2, ' ')) == NULL
     || (f4 = strchr(++f3, ' ')) == NULL) {
	syslog(L_ERROR, "%s bad_format active %s",
	    LogName, MaxLength(start, start));
	return false;
    }
    himark = atol(f2);
    lomark = himark + 1;
    /* note these will be the low and himarks if the group turns out to be empty. */

    /* Check overview data for the group. */
    if (!OVgroupstats(ngp->Name, &low, &high, &count, &flag)) return false;
    if (count != 0) {
	/* non-empty group, so set low/himarks from overview. */
	lomark = low;
	himark = high;
    }
    l = atol(f2);
    if (himark > l) {
	syslog(L_NOTICE, RENUMBER, LogName, ngp->Name, "hi", l, himark);
	if (!FormatLong(f2, himark, f3 - f2 - 1)) {
	    syslog(L_ERROR, NORENUMBER, LogName, ngp->Name, "hi");
	    return false;
	}
	ngp->Last = himark;
	ICDactivedirty++;
    } else if (himark < l) {
        syslog(L_NOTICE, "%s renumber %s hi not decreasing %ld to %ld",
               LogName, ngp->Name, l, himark);
    }
    l = atol(f3);
    if (lomark != l) {
	if (lomark < l)
	    syslog(L_NOTICE, RENUMBER, LogName, ngp->Name, "lo", l, lomark);
	if (!FormatLong(f3, lomark, f4 - f3)) {
	    syslog(L_ERROR, NORENUMBER, LogName, ngp->Name, "lo");
	    return false;
	}
	ICDactivedirty++;
    }
    return true;
}

/*
 * Set the low article count for the given group.
 * Like NGrenumber(), but we don't scan the spool,
 * and the himark is ignored.
 */
bool
NGlowmark(NEWSGROUP *ngp, long lomark)
{
    long l;
    char *f2, *f3, *f4;
    char *start;

    start = ICDreadactive(&f2) + ngp->Start;
    /* Check the file format. */
    if ((f2 = strchr(start, ' ')) == NULL
     || (f3 = strchr(++f2, ' ')) == NULL
     || (f4 = strchr(++f3, ' ')) == NULL) {
        syslog(L_ERROR, "%s bad_format active %s",
            LogName, MaxLength(start, start));
        return false;
    }
    l = atol(f3);
    if (lomark != l) {
        if (lomark < l)
            syslog(L_NOTICE, RENUMBER, LogName, ngp->Name, "lo", l, lomark);
        if (!FormatLong(f3, lomark, f4 - f3)) {
            syslog(L_ERROR, NORENUMBER, LogName, ngp->Name, "lo");
            return false;
        }
        ICDactivedirty++;
    }
    return true;
}
