/*  $Id$
**
**  Code for overview-driven expiration.
**
**  In order to expire on a per-newsgroup (instead of per-storage-class)
**  basis, one has to use overview-driven expiration.  This contains all of
**  the code to do that.  It provides OVgroupbasedexpire, OVhisthasmsgid, and
**  OVgroupmatch for the use of various overview methods.
*/

#include "config.h"
#include "clibrary.h"
#include <ctype.h>
#include <errno.h>

#include "inn/innconf.h"
#include "inn/libinn.h"
#include "inn/ov.h"
#include "ovinterface.h"
#include "inn/paths.h"
#include "inn/storage.h"
#include "inn/vector.h"

enum KRP {Keep, Remove, Poison};

/* Statistics */
long             EXPprocessed;
long             EXPunlinked;
long             EXPoverindexdrop;

#define NGH_HASH(Name, p, j)    \
        for (p = Name, j = 0; *p; ) j = (j << 5) + j + *p++
#define NGH_SIZE        2048
#define NGH_BUCKET(j)   &NGHtable[j & (NGH_SIZE - 1)]

#define OVFMT_UNINIT    -2
#define OVFMT_NODATE    -1
#define OVFMT_NOXREF    -1

static int              Dateindex = OVFMT_UNINIT; 
static int              Xrefindex = OVFMT_UNINIT;
static int              Messageidindex = OVFMT_UNINIT;

typedef struct _NEWSGROUP {
    char                *Name;
    char                *Rest;
    unsigned long       Last;
    unsigned long       Lastpurged;
        /* These fields are new. */
    time_t              Keep;
    time_t              Default;
    time_t              Purge;
    /* X flag => remove entire article when it expires in this group */
    bool                Poison;
} NEWSGROUP;

typedef struct _NGHASH {
    int         Size;
    int         Used;
    NEWSGROUP   **Groups;
} NGHASH;

#define MAGIC_TIME      49710.

typedef struct _BADGROUP {
    struct _BADGROUP    *Next;
    char                *Name;
} BADGROUP;

/*
**  Information about the schema of the news overview files.
*/
typedef struct _ARTOVERFIELD {  
    char        *Header;
    int         Length;
    bool        HasHeader;
    bool        NeedsHeader;
} ARTOVERFIELD;

static BADGROUP         *EXPbadgroups;
static int              nGroups;
static NEWSGROUP        *Groups;
static NEWSGROUP        EXPdefault;
static NGHASH           NGHtable[NGH_SIZE];

static char             **arts;
static enum KRP         *krps;

static ARTOVERFIELD *   ARTfields;
static int              ARTfieldsize;
static bool             ReadOverviewfmt = false;


/* FIXME: The following variables are shared between this file and ov.c.
   This should be cleaned up with a better internal interface. */
time_t   OVnow;
char *   ACTIVE;
FILE *   EXPunlinkfile;
bool     OVignoreselfexpire;
bool     OVusepost;
bool     OVkeep;
bool     OVearliest;
bool     OVquiet;
int      OVnumpatterns;
char **  OVpatterns;


/*
**  Hash a newsgroup and see if we get it.
*/
static NEWSGROUP *
NGfind(char *Name)
{
    char                *p;
    int                 i;
    unsigned int        j;
    NEWSGROUP           **ngp;
    char                c;
    NGHASH              *htp;

    NGH_HASH(Name, p, j);
    htp = NGH_BUCKET(j);
    for (c = *Name, ngp = htp->Groups, i = htp->Used; --i >= 0; ngp++)
        if (c == ngp[0]->Name[0] && strcmp(Name, ngp[0]->Name) == 0)
            return ngp[0];
    return NULL;
}

/*
**  Sorting predicate to put newsgroups in rough order of their activity.
*/
static int
NGcompare(const void *p1, const void *p2)
{
    const NEWSGROUP * const * ng1 = p1;
    const NEWSGROUP * const * ng2 = p2;

    return ng1[0]->Last - ng2[0]->Last;
}

/*
**  Split a line at a specified field separator into a vector and return
**  the number of fields found, or -1 on error.
*/
static int
EXPsplit(char *p, char sep, char **argv, int count)
{
    int i;

    if (!p)
      return 0;

    while (*p == sep)
      ++p;

    if (!*p)
      return 0;

    if (!p)
      return 0;

    while (*p == sep)
      ++p;

    if (!*p)
      return 0;

    for (i = 1, *argv++ = p; *p; )
        if (*p++ == sep) {
            p[-1] = '\0';
            for (; *p == sep; p++);
            if (!*p)
                return i;
            if (++i == count)
                /* Overflow. */
                return -1;
            *argv++ = p;
        }
    return i;
}

/*
**  Build the newsgroup structures from the active file.
*/
static void
BuildGroups(char *active)
{
    NGHASH              *htp;
    NEWSGROUP           *ngp;
    char                *p;
    char                *q;
    int                 i;
    unsigned            j;
    int                 lines;
    int                 NGHbuckets;
    char                *fields[5];

    /* Count the number of groups. */
    for (p = active, i = 0; (p = strchr(p, '\n')) != NULL; p++, i++)
        continue;
    nGroups = i;
    Groups = xmalloc(i * sizeof(NEWSGROUP));

    /* Set up the default hash buckets. */
    NGHbuckets = i / NGH_SIZE;
    if (NGHbuckets == 0)
        NGHbuckets = 1;
    for (i = NGH_SIZE, htp = NGHtable; --i >= 0; htp++) {
        htp->Size = NGHbuckets;
        htp->Groups = xmalloc(htp->Size * sizeof(NEWSGROUP *));
        htp->Used = 0;
    }

    /* Fill in the array. */
    lines = 0;
    for (p = active, ngp = Groups, i = nGroups; --i >= 0; ngp++, p = q + 1) {
        lines++;
        if ((q = strchr(p, '\n')) == NULL) {
            fprintf(stderr, "%s: line %d missing newline\n", ACTIVE, lines);
            exit(1);
        }
	if (*p == '.')
	     continue;
        *q = '\0';
        if (EXPsplit(p, ' ', fields, ARRAY_SIZE(fields)) != 4) {
            fprintf(stderr, "%s: line %d wrong number of fields\n", ACTIVE, lines);
            exit(1);
        }
        ngp->Name = fields[0];
        ngp->Last = atol(fields[1]);
        ngp->Rest = fields[3];

        /* Find the right bucket for the group, make sure there is room. */
        NGH_HASH(ngp->Name, p, j);
        htp = NGH_BUCKET(j);
        if (htp->Used >= htp->Size) {
            htp->Size += NGHbuckets;
            htp->Groups = xrealloc(htp->Groups, htp->Size * sizeof(NEWSGROUP *));
        }
        htp->Groups[htp->Used++] = ngp;
    }

    /* Sort each hash bucket. */
    for (i = NGH_SIZE, htp = NGHtable; --i >= 0; htp++)
    if (htp->Used > 1)
        qsort(htp->Groups, htp->Used, sizeof htp->Groups[0], NGcompare);

    /* Ok, now change our use of the Last field.  Set them all to maxint. */
    for (i = NGH_SIZE, htp = NGHtable; --i >= 0; htp++) {
        NEWSGROUP       **ngpa;
        int             k;

        for (ngpa = htp->Groups, k = htp->Used; --k >= 0; ngpa++) {
            ngpa[0]->Last = ~(unsigned long) 0;
            ngpa[0]->Lastpurged = 0;
        }
    }
}

/*
**  Parse a number field converting it into a "when did this start?".
**  This makes the "keep it" tests fast, but inverts the logic of
**  just about everything you expect.  Print a message and return false
**  on error.
*/
static bool
EXPgetnum(int line, char *word, time_t *v, const char *name)
{
    char                *p;
    bool                SawDot;
    double              d;

    if (strcasecmp(word, "never") == 0) {
        *v = (time_t)0;
        return true;
    }

    /* Check the number.  We don't have strtod yet. */
    for (p = word; ISWHITE(*p); p++)
        continue;
    if (*p == '+' || *p == '-')
        p++;
    for (SawDot = false; *p; p++)
        if (*p == '.') {
            if (SawDot)
                break;
            SawDot = true;
        }
        else if (!CTYPE(isdigit, (int)*p))
            break;
    if (*p) {
        fprintf(stderr, "Line %d, bad `%c' character in %s field\n",
                line, *p, name);
        return false;
    }
    d = atof(word);
    if (d > MAGIC_TIME)
        *v = (time_t)0;
    else
        *v = OVnow - (time_t)(d * 86400.);
    return true;
}

/*
**  Set the expiration fields for all groups that match this pattern.
*/
static void
EXPmatch(char *p, NEWSGROUP *v, char mod)
{
    NEWSGROUP           *ngp;
    int                 i;
    bool                negate;

    negate = *p == '!';
    if (negate)
        p++;
    for (ngp = Groups, i = nGroups; --i >= 0; ngp++)
        if (negate ? !uwildmat(ngp->Name, p) : uwildmat(ngp->Name, p))
            if (mod == 'a'
             || (mod == 'm' && ngp->Rest[0] == NF_FLAG_MODERATED)
             || (mod == 'u' && ngp->Rest[0] != NF_FLAG_MODERATED)) {
                ngp->Keep      = v->Keep;
                ngp->Default   = v->Default;
                ngp->Purge     = v->Purge;
                ngp->Poison    = v->Poison;
            }
}

/*
**  Parse the expiration control file.  Return true if okay.
*/
static bool
EXPreadfile(FILE *F)
{
    char                *p;
    int                 i;
    int                 j;
    int                 k;
    char                mod;
    NEWSGROUP           v;
    bool                SawDefault;
    char                buff[BUFSIZ];
    char                *fields[7];
    char                **patterns;

    /* Scan all lines. */
    SawDefault = false;
    patterns = xmalloc(nGroups * sizeof(char *));
    
    for (i = 1; fgets(buff, sizeof buff, F) != NULL; i++) {
        if ((p = strchr(buff, '\n')) == NULL) {
            fprintf(stderr, "Line %d too long\n", i);
            free(patterns);
            return false;
        }
        *p = '\0';
        p = strchr(buff, '#');
        if (p)
            *p = '\0';
        else
            p = buff + strlen(buff);
        while (--p >= buff) {
            if (isspace((int)*p))
                *p = '\0';
            else
                break;
        }
        if (buff[0] == '\0')
            continue;
        if ((j = EXPsplit(buff, ':', fields, ARRAY_SIZE(fields))) == -1) {
            fprintf(stderr, "Line %d too many fields\n", i);
            free(patterns);
            return false;
        }

        /* Expired-article remember line? */
        if (strcmp(fields[0], "/remember/") == 0) {
            continue;
        }

        /* Regular expiration line -- right number of fields? */
        if (j != 5) {
            fprintf(stderr, "Line %d bad format\n", i);
            free(patterns);
            return false;
        }

        /* Parse the fields. */
        if (strchr(fields[1], 'M') != NULL)
            mod = 'm';
        else if (strchr(fields[1], 'U') != NULL)
            mod = 'u';
        else if (strchr(fields[1], 'A') != NULL)
            mod = 'a';
        else {
            fprintf(stderr, "Line %d bad modflag\n", i);
            free(patterns);
            return false;
        }
        v.Poison = (strchr(fields[1], 'X') != NULL);
        if (!EXPgetnum(i, fields[2], &v.Keep,    "keep")
         || !EXPgetnum(i, fields[3], &v.Default, "default")
         || !EXPgetnum(i, fields[4], &v.Purge,   "purge")) {
            free(patterns);
            return false;
        }
        /* These were turned into offsets, so the test is the opposite
         * of what you think it should be.  If Purge isn't forever,
         * make sure it's greater then the other two fields. */
        if (v.Purge) {
            /* Some value not forever; make sure other values are in range. */
            if (v.Keep && v.Keep < v.Purge) {
                fprintf(stderr, "Line %d keep>purge\n", i);
                free(patterns);
                return false;
            }
            if (v.Default && v.Default < v.Purge) {
                fprintf(stderr, "Line %d default>purge\n", i);
                free(patterns);
                return false;
            }
        }

        /* Is this the default line? */
        if (fields[0][0] == '*' && fields[0][1] == '\0' && mod == 'a') {
            if (SawDefault) {
                fprintf(stderr, "Line %d duplicate default\n", i);
                free(patterns);
                return false;
            }
            EXPdefault.Keep    = v.Keep;
            EXPdefault.Default = v.Default;
            EXPdefault.Purge   = v.Purge;
            EXPdefault.Poison  = v.Poison;
            SawDefault = true;
        }

        /* Assign to all groups that match the pattern and flags. */
        if ((j = EXPsplit(fields[0], ',', patterns, nGroups)) == -1) {
            fprintf(stderr, "Line %d too many patterns\n", i);
            free(patterns);
            return false;
        }
        for (k = 0; k < j; k++)
            EXPmatch(patterns[k], &v, mod);
    }
    free(patterns);

    return true;
}

/*
**  Handle a newsgroup that isn't in the active file.
*/
static NEWSGROUP *
EXPnotfound(char *Entry)
{
    static NEWSGROUP    Removeit;
    BADGROUP            *bg;

    /* See if we already know about this group. */
    for (bg = EXPbadgroups; bg; bg = bg->Next)
        if (strcmp(Entry, bg->Name) == 0)
            break;
    if (bg == NULL) {
        bg = xmalloc(sizeof(BADGROUP));
        bg->Name = xstrdup(Entry);
        bg->Next = EXPbadgroups;
        EXPbadgroups = bg;
    }
    /* remove it all now. */
    if (Removeit.Keep == 0) {
        Removeit.Keep = OVnow;
        Removeit.Default = OVnow;
        Removeit.Purge = OVnow;
    }
    return &Removeit;
}

/*
**  Should we keep the specified article?
*/
static enum KRP
EXPkeepit(char *Entry, time_t when, time_t expires)
{
    NEWSGROUP           *ngp;
    enum KRP            retval = Remove;

    if ((ngp = NGfind(Entry)) == NULL)
        ngp = EXPnotfound(Entry);

    /* Bad posting date? */
    if (when > OVrealnow + 86400) {
        /* Yes -- force the article to go right now. */
        when = expires ? ngp->Purge : ngp->Default;
    }

    /* If no expiration, make sure it wasn't posted before the default. */
    if (expires == 0) {
        if (when >= ngp->Default)
            retval = Keep;

    /* Make sure it's not posted before the purge cut-off and
     * that it's not due to expire. */
    } else {
        if (when >= ngp->Purge && (expires >= OVnow || when >= ngp->Keep))
            retval = Keep;
    }
    if (retval == Keep) {
        return Keep;
    } else {
        return ngp->Poison ? Poison : Remove;
    }
}

/*
**  An article can be removed.  Either print a note, or actually remove it.
**  Takes in the Xref information so that it can pass this to the storage
**  API callback used to generate the list of files to remove.
*/
void
OVEXPremove(TOKEN token, bool deletedgroups, char **xref, int ngroups)
{
    EXPunlinked++;
    if (deletedgroups) {
        EXPprocessed++;
        EXPoverindexdrop++;
    }
    if (EXPunlinkfile && xref != NULL) {
        SMprintfiles(EXPunlinkfile, token, xref, ngroups);
        if (!ferror(EXPunlinkfile))
            return;
        fprintf(stderr, "Can't write to -z file, %s\n", strerror(errno));
        fprintf(stderr, "(Will ignore it for rest of run.)\n");
        fclose(EXPunlinkfile);
        EXPunlinkfile = NULL;
    }
    if (!SMcancel(token) && SMerrno != SMERR_NOENT && SMerrno != SMERR_UNINIT)
        fprintf(stderr, "Can't unlink %s: %s\n", TokenToText(token),
                SMerrorstr);
}

/*
**  Read the overview schema.
*/
static void
ARTreadschema(void)
{
    const struct cvector        *standardoverview;
    const struct vector         *extraoverview;
    ARTOVERFIELD                *fp;
    unsigned int                i;

    /* Count the number of overview fields and allocate ARTfields. */
    standardoverview = overview_fields();
    extraoverview = overview_extra_fields(true);
    ARTfields = xmalloc((standardoverview->count + extraoverview->count + 1)
                        * sizeof(ARTOVERFIELD));

    /* Parse each field. */
    for (i = 0, fp = ARTfields; i < standardoverview->count; i++) {
        fp->NeedsHeader = false;
        fp->HasHeader = false;
        fp->Header = xstrdup(standardoverview->strings[i]);
        fp->Length = strlen(standardoverview->strings[i]);
        fp++;
    }
    for (i = 0; i < extraoverview->count; i++) {
        fp->NeedsHeader = true;
        fp->HasHeader = false;
        fp->Header = xstrdup(extraoverview->strings[i]);
        fp->Length = strlen(extraoverview->strings[i]);
        fp++;
    }

    ARTfieldsize = fp - ARTfields;
}

/*
**  Return a field from the overview line or NULL on error.  Return a copy
**  since we might be re-using the line later.
*/
static char *
OVERGetHeader(const char *p, int field)
{
    static char         *buff;
    static int          buffsize;
    int                 i;
    ARTOVERFIELD        *fp;
    char                *next;

    fp = &ARTfields[field];

    /* Skip leading headers. */
    for (; field-- >= 0 && *p; p++)
        if ((p = strchr(p, '\t')) == NULL)
            return NULL;
    if (*p == '\0')
        return NULL;

    if (fp->HasHeader)
        p += fp->Length + 2;

    if (fp->NeedsHeader) {              /* find an exact match */
         while (strncmp(fp->Header, p, fp->Length) != 0) {
              if ((p = strchr(p, '\t')) == NULL) 
                return NULL;
              p++;
         }
         p += fp->Length + 2;
    }

    /* Figure out length; get space. */
    if ((next = strpbrk(p, "\n\r\t")) != NULL) {
        i = next - p;
    } else {
        i = strlen(p);
    }
    if (buffsize == 0) {
        buffsize = i;
        buff = xmalloc(buffsize + 1);
    }
    else if (buffsize < i) {
        buffsize = i;
        buff = xrealloc(buff, buffsize + 1);
    }

    strncpy(buff, p, i);
    buff[i] = '\0';
    return buff;
}

/*
**  Read overview schema and find index for headers.
*/
static void
OVfindheaderindex(void)
{
    FILE        *F;
    char        *active;
    char        *path;
    int         i;

    if (ReadOverviewfmt)
        return;
    if (innconf->groupbaseexpiry) {
        ACTIVE = concatpath(innconf->pathdb, INN_PATH_ACTIVE);
        if ((active = ReadInFile(ACTIVE, (struct stat *)NULL)) == NULL) {
            fprintf(stderr, "Can't read %s, %s\n",
            ACTIVE, strerror(errno));
            exit(1);
        }
        BuildGroups(active);
        arts = xmalloc(nGroups * sizeof(char *));
        krps = xmalloc(nGroups * sizeof(enum KRP));
        path = concatpath(innconf->pathetc, INN_PATH_EXPIRECTL);
        F = fopen(path, "r");
        free(path);
        if (!EXPreadfile(F)) {
            fclose(F);
            fprintf(stderr, "Format error in expire.ctl\n");
            exit(1);
        }
        fclose(F);
    }
    ARTreadschema();
    if (Dateindex == OVFMT_UNINIT) {
        for (Dateindex = OVFMT_NODATE, i = 0; i < ARTfieldsize; i++) {
            if (strcasecmp(ARTfields[i].Header, "Date") == 0) {
                Dateindex = i;
            } else if (strcasecmp(ARTfields[i].Header, "Xref") == 0) {
                Xrefindex = i;
            } else if (strcasecmp(ARTfields[i].Header, "Message-ID") == 0) {
                Messageidindex = i;
            }
        }
    }
    ReadOverviewfmt = true;
    return;
}

/*
**  Do the work of expiring one line.  Assumes article still exists in the
**  spool.  Returns true if article should be purged, or return false.
*/
bool
OVgroupbasedexpire(TOKEN token, const char *group, const char *data,
                   int len UNUSED, time_t arrived, time_t expires)
{
    static char         *Group = NULL;
    char                *p;
    int                 i;
    int                 count;
    time_t              when;
    bool                poisoned;
    bool                keeper;
    bool                delete;
    bool                purge;
    char                *Xref;

    if (SMprobe(SELFEXPIRE, &token, NULL)) {
        if (!OVignoreselfexpire)
            /* this article should be kept */
            return false;
    }
    if (!ReadOverviewfmt) {
        OVfindheaderindex();
    }

    if (OVusepost) {
        if ((p = OVERGetHeader(data, Dateindex)) == NULL) {
            EXPoverindexdrop++;
            return true;
        }
        when = parsedate_rfc5322_lax(p);
        if (when == (time_t) -1) {
            EXPoverindexdrop++;
            return true;
        }
    } else {
        when = arrived;
    }
    if ((Xref = OVERGetHeader(data, Xrefindex)) == NULL) {
        if (Group != NULL) {
            free(Group);
        }
        Group = concat(group, ":", (char *) 0);
        Xref = Group;
    } else {
        if ((Xref = strchr(Xref, ' ')) == NULL) {
            EXPoverindexdrop++;
            return true;
        }
        for (Xref++; *Xref == ' '; Xref++)
            ;
    }
    if ((count = EXPsplit(Xref, ' ', arts, nGroups)) == -1) {
        EXPoverindexdrop++;
        return true;
    }

    /* arts is now an array of strings, each of which is a group name, a
       colon, and an article number.  EXPkeepit wants just pure group names,
       so replace the colons with nuls (deleting the overview entry if it
       isn't in the expected form). */
    for (i = 0; i < count; i++) {
        p = strchr(arts[i], ':');
        if (p == NULL) {
            fflush(stdout);
            fprintf(stderr, "Bad entry, \"%s\"\n", arts[i]);
            EXPoverindexdrop++;
            return true;
        }
        *p = '\0';
    }

    /* First check all postings */
    poisoned = false;
    keeper = false;
    delete = false;
    purge = true;
    for (i = 0; i < count; ++i) {
        if ((krps[i] = EXPkeepit(arts[i], when, expires)) == Poison)
            poisoned = true;
        if (OVkeep && (krps[i] == Keep))
            keeper = true;
        if ((krps[i] == Remove) && strcmp(group, arts[i]) == 0)
            delete = true;
        if ((krps[i] == Keep))
            purge = false;
    }
    EXPprocessed++;

    if (OVearliest) {
        if (delete || poisoned || token.type == TOKEN_EMPTY) {
            /* delete article if this is first entry */
            if (strcmp(group, arts[0]) == 0) {
                for (i = 0; i < count; i++)
                    arts[i][strlen(arts[i])] = ':';
                OVEXPremove(token, false, arts, count);
            }
            EXPoverindexdrop++;
            return true;
        }
    } else { /* not earliest mode */
        if ((!keeper && delete) || token.type == TOKEN_EMPTY) {
            /* delete article if purge is set, indicating that it has
               expired out of every group to which it was posted */
            if (purge) {
                for (i = 0; i < count; i++)
                    arts[i][strlen(arts[i])] = ':';
                OVEXPremove(token, false, arts, count);
            }
            EXPoverindexdrop++;
            return true;
        }
    }

    /* this article should be kept */
    return false;
}

bool
OVhisthasmsgid(struct history *h, const char *data)
{
    char *p;

    if (!ReadOverviewfmt) {
        OVfindheaderindex();
    }
    if ((p = OVERGetHeader(data, Messageidindex)) == NULL)
        return false;
    return HISlookup(h, p, NULL, NULL, NULL, NULL);
}

bool
OVgroupmatch(const char *group)
{
    int i;
    bool wanted = false;

    if (OVnumpatterns == 0 || group == NULL)
        return true;
    for (i = 0; i < OVnumpatterns; i++) {
        switch (OVpatterns[i][0]) {
        case '!':
            if (!wanted && uwildmat(group, &OVpatterns[i][1]))
                break;
        case '@':
            if (uwildmat(group, &OVpatterns[i][1])) {
                return false;
            }
            break;
        default:
            if (uwildmat(group, OVpatterns[i]))
                wanted = true;
        }
    }
    return wanted;
}

void
OVEXPcleanup(void)
{
    int i;
    BADGROUP *bg, *bgnext;
    ARTOVERFIELD *fp;
    NGHASH *htp;

    if (EXPprocessed != 0) {
        if (!OVquiet) {
            printf("Article lines processed %8ld\n", EXPprocessed);
            printf("Articles dropped        %8ld\n", EXPunlinked);
            printf("Overview index dropped  %8ld\n", EXPoverindexdrop);
        }
        EXPprocessed = EXPunlinked = EXPoverindexdrop = 0;
    }
    if (innconf->ovgrouppat != NULL) {
        for (i = 0 ; i < OVnumpatterns ; i++)
            free(OVpatterns[i]);
        free(OVpatterns);
    }
    for (bg = EXPbadgroups; bg; bg = bgnext) {
        bgnext = bg->Next;
        free(bg->Name);
        free(bg);
    }
    for (fp = ARTfields, i = 0; i < ARTfieldsize ; i++, fp++) {
        free(fp->Header);
    }
    free(ARTfields);
    if (ACTIVE != NULL) {
        free(ACTIVE);
        ACTIVE = NULL;
    }
    if (Groups != NULL) {
        free(Groups);
        Groups = NULL;
    }
    for (i = 0, htp = NGHtable ; i < NGH_SIZE ; i++, htp++) {
        if (htp->Groups != NULL) {
            free(htp->Groups);
            htp->Groups = NULL;
        }
    }
}
