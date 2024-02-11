/*
**  Storage manager module for Cyclic News File System method.
*/

#include "portable/system.h"

#include "portable/mmap.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#if HAVE_LIMITS_H
#    include <limits.h>
#endif
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/uio.h>

#ifdef HAVE_SYS_TIME_H
#    include <sys/time.h>
#endif
#include <time.h>

#include "inn/fdflag.h"
#include "inn/innconf.h"
#include "inn/libinn.h"
#include "inn/messages.h"
#include "inn/mmap.h"
#include "inn/paths.h"
#include "inn/wire.h"
#include "interface.h"
#include "methods.h"

#include "cnfs-private.h"
#include "cnfs.h"

/* Temporary until cnfs_mapcntl is handled like msync_page.  Make MS_ASYNC
   disappear on platforms that don't have it. */
#ifndef MS_ASYNC
#    define MS_ASYNC 0
#endif

/* We can give a more descriptive error below about not having largefile
   support if the platform has EOVERFLOW; on other platforms some other
   errno will be used and so we won't know when to give the descriptive
   error.  Oh well. */
#ifndef EOVERFLOW
#    define EOVERFLOW 0
#endif

typedef struct {
    /**** Stuff to be cleaned up when we're done with the article */
    char *base;       /* Base of mmap()ed art */
    int len;          /* Length of article (and thus
                         mmap()ed art */
    CYCBUFF *cycbuff; /* pointer to current CYCBUFF */
    off_t offset;     /* offset to current article */
    bool rollover;    /* true if the search is rollovered */
} PRIV_CNFS;

static CYCBUFF *cycbufftab = (CYCBUFF *) NULL;
static METACYCBUFF *metacycbufftab = (METACYCBUFF *) NULL;
static CNFSEXPIRERULES *metaexprulestab = (CNFSEXPIRERULES *) NULL;
static long pagesize = 0;
static int metabuff_update = METACYCBUFF_UPDATE;
static int refresh_interval = REFRESH_INTERVAL;

static CYCBUFF *CNFSgetcycbuffbyname(char *name);


/*
**  The token is @03nnxxxxxxxxxxxxxxxxyyyyyyyyzzzzzzzz@
**  where "03" is the cnfs method number,
**  "nn" the hexadecimal value of the storage class,
**  "xxxxxxxxxxxxxxxx" the name of the cyclic buffer (as defined
**  in <pathetc>/cycbuff.conf),
**  "yyyyyyyy" the block,
**  "zzzzzzzz" the cyclic number.
*/
char *
cnfs_explaintoken(const TOKEN token)
{
    char *text;
    CYCBUFF *cycbuff;
    char cycbuffname[CNFSMAXCYCBUFFNAME + 1];
    unsigned int blksz;
    uint32_t block;
    uint32_t cycnum;

    snprintf(cycbuffname, sizeof(cycbuffname), "%.*s",
             (int) sizeof(cycbuffname) - 1, token.token);

    if ((cycbuff = CNFSgetcycbuffbyname(cycbuffname)) == NULL) {
        blksz = CNFS_DFL_BLOCKSIZE;
    } else {
        blksz = (unsigned int) cycbuff->blksz;
    }

    memcpy(&block, &token.token[8], sizeof(block));
    memcpy(&cycnum, &token.token[12], sizeof(cycnum));
    xasprintf(&text,
              "method=cnfs class=%u buffer=%s block=%lu blocksize=%u "
              "cycnum=%lu file=%s",
              (unsigned int) token.class, cycbuffname,
              (unsigned long) ntohl(block), blksz,
              (unsigned long) ntohl(cycnum), cycbuff ? cycbuff->path : "");

    return text;
}


static TOKEN
CNFSMakeToken(char *cycbuffname, off_t offset, int blksz, uint32_t cycnum,
              STORAGECLASS class)
{
    TOKEN token;
    uint32_t uint32;

    token.type = TOKEN_CNFS;
    token.class = class;
    memcpy(token.token, cycbuffname, CNFSMAXCYCBUFFNAME);
    uint32 = htonl(offset / blksz);
    memcpy(&token.token[8], &uint32, sizeof(uint32));
    uint32 = htonl(cycnum);
    memcpy(&token.token[12], &uint32, sizeof(uint32));
    return token;
}

/*
** NOTE: We assume that cycbuffname is 9 bytes long.
*/
static bool
CNFSBreakToken(TOKEN token, char *cycbuffname, uint32_t *blk, uint32_t *cycnum)
{
    uint32_t uint32;

    if (cycbuffname == NULL || blk == NULL || cycnum == NULL) {
        if (cycbuffname == NULL) {
            warn("CNFS: BreakToken: invalid argument");
        } else {
            warn("CNFS: BreakToken: invalid argument: %s", cycbuffname);
        }
        SMseterror(SMERR_INTERNAL, "BreakToken: invalid argument");
        return false;
    }
    memcpy(cycbuffname, token.token, CNFSMAXCYCBUFFNAME);
    *(cycbuffname + CNFSMAXCYCBUFFNAME) = '\0'; /* Just to be paranoid */
    memcpy(&uint32, &token.token[8], sizeof(uint32));
    *blk = ntohl(uint32);
    memcpy(&uint32, &token.token[12], sizeof(uint32));
    *cycnum = ntohl(uint32);
    return true;
}

static char hextbl[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

/*
** CNFSofft2hex -- Given an argument of type off_t, return
** a static ASCII string representing its value in hexadecimal.
**
** If "leadingzeros" is true, the number returned will have leading 0's.
*/

static char *
CNFSofft2hex(off_t offset, bool leadingzeros)
{
    static char buf[24];
    char *p;

    if (sizeof(off_t) <= sizeof(unsigned long)) {
        snprintf(buf, sizeof(buf), (leadingzeros) ? "%016lx" : "%lx",
                 (unsigned long) offset);
    } else {
        int i;

        for (i = 0; i < CNFSLASIZ; i++)
            buf[i] = '0'; /* Pad with zeros to start */
        for (i = CNFSLASIZ - 1; i >= 0; i--) {
            buf[i] = hextbl[offset & 0xf];
            offset >>= 4;
        }
    }
    if (!leadingzeros) {
        for (p = buf; *p == '0'; p++)
            ;
        if (*p != '\0')
            return p;
        else
            return p - 1; /* We converted a "0" and then bypassed all
                             the zeros */
    } else
        return buf;
}

/*
** CNFShex2offt -- Given an ASCII string containing a hexadecimal
** representation of a off_t, return a off_t.
*/

static off_t
CNFShex2offt(char *hex)
{
    if (sizeof(off_t) <= 4) {
        unsigned long rpofft;
        /* I'm lazy */
        sscanf(hex, "%lx", &rpofft);
        return rpofft;
    } else {
        char diff;
        off_t n = 0;

        for (; *hex != '\0'; hex++) {
            if (*hex >= '0' && *hex <= '9')
                diff = '0';
            else if (*hex >= 'a' && *hex <= 'f')
                diff = 'a' - 10;
            else if (*hex >= 'A' && *hex <= 'F')
                diff = 'A' - 10;
            else {
                /*
                ** We used to have a syslog() message here, but the case
                ** where we land here because of a ":" happens, er, often.
                */
                break;
            }
            n += (*hex - diff);
            if (isalnum((unsigned char) *(hex + 1)))
                n <<= 4;
        }
        return n;
    }
}

static bool
CNFSflushhead(CYCBUFF *cycbuff)
{
    CYCBUFFEXTERN rpx;

    if (!cycbuff->needflush)
        return true;
    if (!SMopenmode) {
        warn("CNFS: CNFSflushhead: attempted flush whilst read only");
        return false;
    }
    memset(&rpx, 0, sizeof(CYCBUFFEXTERN));
    if (cycbuff->magicver == 3 || cycbuff->magicver == 4) {
        cycbuff->updated = time(NULL);
        /* Don't use sprintf() or strlcat() directly...
         * The terminating '\0' causes grief. */
#if __GNUC__ > 7
#    pragma GCC diagnostic ignored "-Wstringop-truncation"
#endif
        if (cycbuff->magicver == 3)
            strncpy(rpx.magic, CNFS_MAGICV3, strlen(CNFS_MAGICV3));
        else
            strncpy(rpx.magic, CNFS_MAGICV4, strlen(CNFS_MAGICV4));
        strncpy(rpx.name, cycbuff->name, CNFSNASIZ);
        strncpy(rpx.path, cycbuff->path, CNFSPASIZ);
        strncpy(rpx.lena, CNFSofft2hex(cycbuff->len, true), CNFSLASIZ);
        strncpy(rpx.freea, CNFSofft2hex(cycbuff->free, true), CNFSLASIZ);
        strncpy(rpx.cyclenuma, CNFSofft2hex(cycbuff->cyclenum, true),
                CNFSLASIZ);
        strncpy(rpx.updateda, CNFSofft2hex(cycbuff->updated, true), CNFSLASIZ);
        strncpy(rpx.metaname, cycbuff->metaname, CNFSNASIZ);
        strncpy(rpx.orderinmeta, CNFSofft2hex(cycbuff->order, true),
                CNFSLASIZ);
        if (cycbuff->currentbuff) {
            strncpy(rpx.currentbuff, "TRUE", CNFSMASIZ);
        } else {
            strncpy(rpx.currentbuff, "FALSE", CNFSMASIZ);
        }
        strncpy(rpx.blksza, CNFSofft2hex(cycbuff->blksz, true), CNFSLASIZ);
#if __GNUC__ > 7
#    pragma GCC diagnostic warning "-Wstringop-truncation"
#endif
        memcpy(cycbuff->bitfield, &rpx, sizeof(CYCBUFFEXTERN));
        msync(cycbuff->bitfield, cycbuff->minartoffset, MS_ASYNC);
        cycbuff->needflush = false;
    } else {
        warn("CNFS: CNFSflushhead: bogus magicver for %s: %d", cycbuff->name,
             cycbuff->magicver);
        return false;
    }
    return true;
}

static void
CNFSshutdowncycbuff(CYCBUFF *cycbuff)
{
    if (cycbuff == (CYCBUFF *) NULL)
        return;
    if (cycbuff->needflush) {
        notice("CNFS: CNFSshutdowncycbuff: flushing %s", cycbuff->name);
        CNFSflushhead(cycbuff);
    }
    if (cycbuff->bitfield != NULL) {
        munmap(cycbuff->bitfield, cycbuff->minartoffset);
        cycbuff->bitfield = NULL;
    }
    if (cycbuff->fd >= 0)
        close(cycbuff->fd);
    cycbuff->fd = -1;
}

static void
CNFScleancycbuff(void)
{
    CYCBUFF *cycbuff, *nextcycbuff;

    for (cycbuff = cycbufftab; cycbuff != (CYCBUFF *) NULL;) {
        CNFSshutdowncycbuff(cycbuff);
        nextcycbuff = cycbuff->next;
        free(cycbuff);
        cycbuff = nextcycbuff;
    }
    cycbufftab = (CYCBUFF *) NULL;
}

static void
CNFScleanmetacycbuff(void)
{
    METACYCBUFF *metacycbuff, *nextmetacycbuff;

    for (metacycbuff = metacycbufftab; metacycbuff != (METACYCBUFF *) NULL;) {
        nextmetacycbuff = metacycbuff->next;
        free(metacycbuff->members);
        free(metacycbuff->name);
        free(metacycbuff);
        metacycbuff = nextmetacycbuff;
    }
    metacycbufftab = (METACYCBUFF *) NULL;
}

static void
CNFScleanexpirerule(void)
{
    CNFSEXPIRERULES *metaexprule, *nextmetaexprule;

    for (metaexprule = metaexprulestab;
         metaexprule != (CNFSEXPIRERULES *) NULL;) {
        nextmetaexprule = metaexprule->next;
        free(metaexprule);
        metaexprule = nextmetaexprule;
    }
    metaexprulestab = (CNFSEXPIRERULES *) NULL;
}

static CYCBUFF *
CNFSgetcycbuffbyname(char *name)
{
    CYCBUFF *cycbuff;

    if (name == NULL)
        return NULL;
    for (cycbuff = cycbufftab; cycbuff != (CYCBUFF *) NULL;
         cycbuff = cycbuff->next)
        if (strcmp(name, cycbuff->name) == 0)
            return cycbuff;
    return NULL;
}

static METACYCBUFF *
CNFSgetmetacycbuffbyname(char *name)
{
    METACYCBUFF *metacycbuff;

    if (name == NULL)
        return NULL;
    for (metacycbuff = metacycbufftab; metacycbuff != (METACYCBUFF *) NULL;
         metacycbuff = metacycbuff->next)
        if (strcmp(name, metacycbuff->name) == 0)
            return metacycbuff;
    return NULL;
}

static void
CNFSflushallheads(void)
{
    CYCBUFF *cycbuff;

    for (cycbuff = cycbufftab; cycbuff != (CYCBUFF *) NULL;
         cycbuff = cycbuff->next) {
        if (cycbuff->needflush)
            notice("CNFS: CNFSflushallheads: flushing %s", cycbuff->name);
        CNFSflushhead(cycbuff);
    }
}

/*
** CNFSReadFreeAndCycle() -- Read from disk the current values of CYCBUFF's
** free pointer and cycle number.  Return 1 on success, 0 otherwise.
*/

static void
CNFSReadFreeAndCycle(CYCBUFF *cycbuff)
{
    CYCBUFFEXTERN rpx;
    char buf[64];

    memcpy(&rpx, cycbuff->bitfield, sizeof(CYCBUFFEXTERN));
    /* Sanity checks are not needed since CNFSinit_disks() has already done. */
    strncpy(buf, rpx.freea, CNFSLASIZ);
    buf[CNFSLASIZ] = '\0';
    cycbuff->free = CNFShex2offt(buf);
    strncpy(buf, rpx.updateda, CNFSLASIZ);
    buf[CNFSLASIZ] = '\0';
    cycbuff->updated = CNFShex2offt(buf);
    strncpy(buf, rpx.cyclenuma, CNFSLASIZ);
    buf[CNFSLASIZ] = '\0';
    cycbuff->cyclenum = CNFShex2offt(buf);
    return;
}

static bool
CNFSparse_part_line(char *l)
{
    char *p;
    struct stat sb;
    off_t len;
    CYCBUFF *cycbuff, *tmp;

    /* Symbolic cnfs partition name */
    if ((p = strchr(l, ':')) == NULL || p - l <= 0
        || p - l > CNFSMAXCYCBUFFNAME - 1) {
        warn("CNFS: bad cycbuff name in line '%s'", l);
        return false;
    }
    *p = '\0';
    if (CNFSgetcycbuffbyname(l) != NULL) {
        *p = ':';
        warn("CNFS: duplicate cycbuff name in line '%s'", l);
        return false;
    }
    cycbuff = xmalloc(sizeof(CYCBUFF));
    memset(cycbuff->name, '\0', CNFSNASIZ);
    strlcpy(cycbuff->name, l, CNFSNASIZ);
    l = ++p;

    /* Path to cnfs partition */
    if ((p = strchr(l, ':')) == NULL || p - l <= 0 || p - l > CNFSPASIZ - 1) {
        warn("CNFS: bad pathname in line '%s'", l);
        free(cycbuff);
        return false;
    }
    *p = '\0';
    memset(cycbuff->path, '\0', CNFSPASIZ);
    strlcpy(cycbuff->path, l, CNFSPASIZ);
    if (stat(cycbuff->path, &sb) < 0) {
        if (errno == EOVERFLOW) {
            warn("CNFS: file '%s': Overflow (probably >2GB without largefile"
                 " support), ignoring '%s' cycbuff",
                 cycbuff->path, cycbuff->name);
        } else {
            warn("CNFS: file '%s': %s, ignoring '%s' cycbuff", cycbuff->path,
                 strerror(errno), cycbuff->name);
        }
        free(cycbuff);
        return false;
    }
    l = ++p;

    /* Length/size of symbolic partition */
    len =
        strtoul(l, NULL, 10) * (off_t) 1024; /* This value in KB in decimal */
    if (S_ISREG(sb.st_mode) && len != sb.st_size) {
        if (sizeof(CYCBUFFEXTERN) > (size_t) sb.st_size) {
            notice("CNFS: length must be at least '%lu' for '%s' cycbuff(%lu "
                   "bytes)",
                   (unsigned long) sizeof(CYCBUFFEXTERN), cycbuff->name,
                   (unsigned long) sb.st_size);
            free(cycbuff);
            return false;
        }
    }
    cycbuff->len = len;
    cycbuff->fd = -1;
    cycbuff->next = (CYCBUFF *) NULL;
    cycbuff->needflush = false;
    cycbuff->bitfield = NULL;
    cycbuff->minartoffset = 0;
    if (cycbufftab == (CYCBUFF *) NULL)
        cycbufftab = cycbuff;
    else {
        for (tmp = cycbufftab; tmp->next != (CYCBUFF *) NULL; tmp = tmp->next)
            ;
        tmp->next = cycbuff;
    }
    /* Done! */
    return true;
}

static bool
CNFSparse_metapart_line(char *l)
{
    char *p, *cycbuff, *q = l;
    CYCBUFF *rp;
    METACYCBUFF *metacycbuff, *tmp;

    /* Symbolic metacycbuff name */
    if ((p = strchr(l, ':')) == NULL || p - l <= 0) {
        warn("CNFS: bad partition name in line '%s'", l);
        return false;
    }
    *p = '\0';
    if (CNFSgetmetacycbuffbyname(l) != NULL) {
        *p = ':';
        warn("CNFS: duplicate metabuff name in line '%s'", l);
        return false;
    }
    metacycbuff = xmalloc(sizeof(METACYCBUFF));
    metacycbuff->members = (CYCBUFF **) NULL;
    metacycbuff->count = 0;
    metacycbuff->name = xstrdup(l);
    metacycbuff->next = (METACYCBUFF *) NULL;
    metacycbuff->metamode = INTERLEAVE;
    l = ++p;

    if ((p = strchr(l, ':')) != NULL) {
        if (p - l <= 0) {
            warn("CNFS: bad mode in line '%s'", q);
            return false;
        }
        if (strcmp(++p, "INTERLEAVE") == 0)
            metacycbuff->metamode = INTERLEAVE;
        else if (strcmp(p, "SEQUENTIAL") == 0)
            metacycbuff->metamode = SEQUENTIAL;
        else {
            warn("CNFS: unknown mode in line '%s'", q);
            return false;
        }
        *--p = '\0';
    }
    /* Cycbuff list */
    while ((p = strchr(l, ',')) != NULL && p - l > 0) {
        *p = '\0';
        cycbuff = l;
        l = ++p;
        if ((rp = CNFSgetcycbuffbyname(cycbuff)) == NULL) {
            warn("CNFS: bogus cycbuff '%s' (metacycbuff '%s')", cycbuff,
                 metacycbuff->name);
            free(metacycbuff->members);
            free(metacycbuff->name);
            free(metacycbuff);
            return false;
        }
        if (metacycbuff->count == 0)
            metacycbuff->members = xmalloc(sizeof(CYCBUFF *));
        else
            metacycbuff->members =
                xrealloc(metacycbuff->members,
                         (metacycbuff->count + 1) * sizeof(CYCBUFF *));
        metacycbuff->members[metacycbuff->count++] = rp;
    }
    /* Gotta deal with the last cycbuff on the list */
    cycbuff = l;
    if ((rp = CNFSgetcycbuffbyname(cycbuff)) == NULL) {
        warn("CNFS: bogus cycbuff '%s' (metacycbuff '%s')", cycbuff,
             metacycbuff->name);
        free(metacycbuff->members);
        free(metacycbuff->name);
        free(metacycbuff);
        return false;
    } else {
        if (metacycbuff->count == 0)
            metacycbuff->members = xmalloc(sizeof(CYCBUFF *));
        else
            metacycbuff->members =
                xrealloc(metacycbuff->members,
                         (metacycbuff->count + 1) * sizeof(CYCBUFF *));
        metacycbuff->members[metacycbuff->count++] = rp;
    }

    if (metacycbuff->count == 0) {
        warn("CNFS: no cycbuffs assigned to cycbuff '%s'", metacycbuff->name);
        free(metacycbuff->name);
        free(metacycbuff);
        return false;
    }
    if (metacycbufftab == (METACYCBUFF *) NULL)
        metacycbufftab = metacycbuff;
    else {
        for (tmp = metacycbufftab; tmp->next != (METACYCBUFF *) NULL;
             tmp = tmp->next)
            ;
        tmp->next = metacycbuff;
    }
    /* DONE! */
    return true;
}

static bool
CNFSparse_groups_line(void)
{
    METACYCBUFF *mrp;
    STORAGE_SUB *sub = (STORAGE_SUB *) NULL;
    CNFSEXPIRERULES *metaexprule, *tmp;

    sub = SMGetConfig(TOKEN_CNFS, sub);
    for (; sub != (STORAGE_SUB *) NULL; sub = SMGetConfig(TOKEN_CNFS, sub)) {
        if (sub->options == (char *) NULL) {
            warn("CNFS: storage.conf options field is missing");
            CNFScleanexpirerule();
            return false;
        }
        if ((mrp = CNFSgetmetacycbuffbyname(sub->options)) == NULL) {
            warn("CNFS: storage.conf options field '%s' undefined",
                 sub->options);
            CNFScleanexpirerule();
            return false;
        }
        metaexprule = xmalloc(sizeof(CNFSEXPIRERULES));
        metaexprule->class = sub->class;
        metaexprule->dest = mrp;
        metaexprule->next = (CNFSEXPIRERULES *) NULL;
        if (metaexprulestab == (CNFSEXPIRERULES *) NULL)
            metaexprulestab = metaexprule;
        else {
            for (tmp = metaexprulestab; tmp->next != (CNFSEXPIRERULES *) NULL;
                 tmp = tmp->next)
                ;
            tmp->next = metaexprule;
        }
    }
    /* DONE! */
    return true;
}

/*
** CNFSinit_disks -- Finish initializing cycbufftab
** Called by "innd" only -- we open (and keep) a read/write
** file descriptor for each CYCBUFF.
**
** Calling this function repeatedly shouldn't cause any harm
** speed-wise or bug-wise, as long as the caller is accessing the
** CYCBUFFs _read-only_.  If innd calls this function repeatedly,
** bad things will happen.
*/

static bool
CNFSinit_disks(CYCBUFF *cycbuff)
{
    char buf[64];
    CYCBUFFEXTERN *rpx;
    int fd;
    int tonextblock;
    off_t tmpo;
    off_t minartoffset;
    bool oneshot;

    /*
    ** Discover the state of our cycbuffs.  If any of them are in icky shape,
    ** duck shamelessly & return false.
    */

    if (cycbuff != (CYCBUFF *) NULL)
        oneshot = true;
    else {
        oneshot = false;
        cycbuff = cycbufftab;
    }
    for (; cycbuff != (CYCBUFF *) NULL; cycbuff = cycbuff->next) {
        if (strcmp(cycbuff->path, "/dev/null") == 0) {
            warn("CNFS: ERROR opening '%s' is not available", cycbuff->path);
            return false;
        }
        if (cycbuff->fd < 0) {
            if ((fd = open(cycbuff->path, SMopenmode ? O_RDWR : O_RDONLY))
                < 0) {
                syswarn("CNFS: ERROR opening '%s' O_RDONLY", cycbuff->path);
                return false;
            } else {
                fdflag_close_exec(fd, true);
                cycbuff->fd = fd;
            }
        }
        errno = 0;
        cycbuff->bitfield =
            mmap(NULL, CNFS_HDR_PAGESIZE,
                 SMopenmode ? (PROT_READ | PROT_WRITE) : PROT_READ, MAP_SHARED,
                 cycbuff->fd, 0);
        if (cycbuff->bitfield == MAP_FAILED || errno != 0) {
            syswarn(
                "CNFS: CNFSinitdisks: mmap for %s offset %d len %ld failed",
                cycbuff->path, 0, (long) cycbuff->minartoffset);
            cycbuff->bitfield = NULL;
            return false;
        }

        /*
        ** Much of this checking from previous revisions is (probably) bogus
        ** & buggy & particularly icky & unupdated.  Use at your own risk.  :-)
        */
        rpx = (CYCBUFFEXTERN *) cycbuff->bitfield;
        cycbuff->magicver = 0;
        if (strncmp(rpx->magic, CNFS_MAGICV3, strlen(CNFS_MAGICV3)) == 0) {
            cycbuff->magicver = 3;
            cycbuff->blksz = 512;
        }
        if (strncmp(rpx->magic, CNFS_MAGICV4, strlen(CNFS_MAGICV4)) == 0)
            cycbuff->magicver = 4;
        if (cycbuff->magicver >= 3) {
            if (strncmp(rpx->name, cycbuff->name, CNFSNASIZ) != 0) {
                warn("CNFS: Mismatch 3: read %s for cycbuff %s", rpx->name,
                     cycbuff->name);
                return false;
            }
            if (strncmp(rpx->path, cycbuff->path, CNFSPASIZ) != 0) {
                warn("CNFS: Path mismatch: read %s for cycbuff %s", rpx->path,
                     cycbuff->path);
            }
            strncpy(buf, rpx->lena, CNFSLASIZ);
            buf[CNFSLASIZ] = '\0';
            tmpo = CNFShex2offt(buf);
            if (tmpo != cycbuff->len) {
                warn("CNFS: Mismatch: read 0x%s length for cycbuff %s",
                     CNFSofft2hex(tmpo, false), cycbuff->path);
                return false;
            }
            strncpy(buf, rpx->freea, CNFSLASIZ);
            buf[CNFSLASIZ] = '\0';
            cycbuff->free = CNFShex2offt(buf);
            strncpy(buf, rpx->updateda, CNFSLASIZ);
            buf[CNFSLASIZ] = '\0';
            cycbuff->updated = CNFShex2offt(buf);
            strncpy(buf, rpx->cyclenuma, CNFSLASIZ);
            buf[CNFSLASIZ] = '\0';
            cycbuff->cyclenum = CNFShex2offt(buf);
            strncpy(cycbuff->metaname, rpx->metaname, CNFSNASIZ);
            strncpy(buf, rpx->orderinmeta, CNFSLASIZ);
            cycbuff->order = CNFShex2offt(buf);
            if (strncmp(rpx->currentbuff, "TRUE", CNFSMASIZ) == 0) {
                cycbuff->currentbuff = true;
            } else
                cycbuff->currentbuff = false;
            if (cycbuff->magicver > 3) {
                strncpy(buf, rpx->blksza, CNFSLASIZ);
                buf[CNFSLASIZ] = '\0';
                cycbuff->blksz = CNFShex2offt(buf);
            }
            if (cycbuff->blksz < 512 || cycbuff->blksz > CNFS_MAX_BLOCKSIZE
                || 2 * (cycbuff->blksz / 2) != cycbuff->blksz) {
                warn("CNFS: Invalid: read 0x%s blocksize for cycbuff %s",
                     CNFSofft2hex(cycbuff->blksz, false), cycbuff->path);
                return false;
            }
        } else {
            notice("CNFS: no magic cookie found for cycbuff %s, initializing",
                   cycbuff->name);
            cycbuff->magicver = 4;
            cycbuff->free = cycbuff->minartoffset;
            cycbuff->updated = 0;
            cycbuff->cyclenum = 1;
            cycbuff->currentbuff = true;
            cycbuff->order = 0; /* to indicate this is newly added cycbuff */
            cycbuff->needflush = true;
            cycbuff->blksz = CNFS_DFL_BLOCKSIZE;
            cycbuff->free = 0;
            memset(cycbuff->metaname, '\0', CNFSNASIZ);
        }
        /*
        ** The minimum article offset will be the size of the bitfield itself,
        ** len / (blocksize * 8), plus however many additional blocks the
        *CYCBUFF
        ** external header occupies ... then round up to the next block.
        */
        minartoffset = cycbuff->len / (cycbuff->blksz * 8) + CNFS_BEFOREBITF;
        tonextblock =
            CNFS_HDR_PAGESIZE - (minartoffset & (CNFS_HDR_PAGESIZE - 1));
        cycbuff->minartoffset = minartoffset + tonextblock;

        munmap(cycbuff->bitfield, CNFS_HDR_PAGESIZE);
        errno = 0;
        cycbuff->bitfield =
            mmap(NULL, cycbuff->minartoffset,
                 SMopenmode ? (PROT_READ | PROT_WRITE) : PROT_READ, MAP_SHARED,
                 cycbuff->fd, 0);
        if (cycbuff->bitfield == MAP_FAILED || errno != 0) {
            warn("CNFS: CNFSinitdisks: mmap for %s offset %d len %ld failed: "
                 "%s",
                 cycbuff->path, 0, (long) cycbuff->minartoffset,
                 strerror(errno));
            cycbuff->bitfield = NULL;
            return false;
        }

        if (cycbuff->free == 0)
            cycbuff->free = cycbuff->minartoffset;
        if (cycbuff->needflush && !CNFSflushhead(cycbuff))
            return false;

        if (oneshot)
            break;
    }
    return true;
}

static bool
CNFS_setcurrent(METACYCBUFF *metacycbuff)
{
    CYCBUFF *cycbuff;
    int i, currentcycbuff = 0, order = -1;
    bool foundcurrent = false;
    for (i = 0; i < metacycbuff->count; i++) {
        cycbuff = metacycbuff->members[i];
        if (strncmp(cycbuff->metaname, metacycbuff->name, CNFSNASIZ) != 0) {
            /* this cycbuff is moved from other metacycbuff , or is new */
            cycbuff->order = i + 1;
            cycbuff->currentbuff = false;
            /* Don't use sprintf() or strlcat() directly...
             * The terminating '\0' causes grief. */
#if __GNUC__ > 7
#    pragma GCC diagnostic ignored "-Wstringop-truncation"
#endif
            strncpy(cycbuff->metaname, metacycbuff->name, CNFSNASIZ);
#if __GNUC__ > 7
#    pragma GCC diagnostic warning "-Wstringop-truncation"
#endif
            cycbuff->needflush = true;
            continue;
        }
        if (foundcurrent == false && cycbuff->currentbuff == true) {
            currentcycbuff = i;
            foundcurrent = true;
        }
        if (foundcurrent == false || order == -1 || order > cycbuff->order) {
            /* this cycbuff is a candidate for current cycbuff */
            currentcycbuff = i;
            order = cycbuff->order;
        }
        if (cycbuff->order != i + 1) {
            /* cycbuff order seems to be changed */
            cycbuff->order = i + 1;
            cycbuff->needflush = true;
        }
    }
    /* If no current cycbuff found (say, all our cycbuffs are new) default to 0
     */
    if (foundcurrent == false) {
        currentcycbuff = 0;
    }
    for (i = 0; i < metacycbuff->count; i++) {
        cycbuff = metacycbuff->members[i];
        if (currentcycbuff == i && cycbuff->currentbuff == false) {
            cycbuff->currentbuff = true;
            cycbuff->needflush = true;
        }
        if (currentcycbuff != i && cycbuff->currentbuff == true) {
            cycbuff->currentbuff = false;
            cycbuff->needflush = true;
        }
        if (cycbuff->needflush == true && !CNFSflushhead(cycbuff))
            return false;
    }
    metacycbuff->memb_next = currentcycbuff;
    return true;
}

/*
** CNFSread_config() -- Read the cnfs partition/file configuration file.
**
** Oh, for the want of Perl!  My parser probably shows that I don't use
** C all that often anymore....
*/

static bool
CNFSread_config(void)
{
    char *path, *config, *from, *to, **ctab = (char **) NULL;
    int ctab_free = 0; /* Index to next free slot in ctab */
    int ctab_i;
    bool metacycbufffound = false;
    bool cycbuffupdatefound = false;
    bool refreshintervalfound = false;
    int update, refresh;

    path = concatpath(innconf->pathetc, _PATH_CYCBUFFCONFIG);
    config = ReadInFile(path, NULL);
    if (config == NULL) {
        syswarn("CNFS: cannot read %s", path);
        free(config);
        free(path);
        return false;
    }
    free(path);
    for (from = to = config; *from;) {
        if (*from == '#') { /* Comment line? */
            while (*from && *from != '\n')
                from++; /* Skip past it */
            from++;
            continue; /* Back to top of loop */
        }
        if (*from == '\n') { /* End or just a blank line? */
            from++;
            continue; /* Back to top of loop */
        }
        if (ctab_free == 0)
            ctab = xmalloc(sizeof(char *));
        else
            ctab = xrealloc(ctab, (ctab_free + 1) * sizeof(char *));
        /* If we're here, we've got the beginning of a real entry */
        ctab[ctab_free++] = to = from;
        while (1) {
            if (*from && *from == '\\' && *(from + 1) == '\n') {
                from += 2; /* Skip past backslash+newline */
                while (*from && isspace((unsigned char) *from))
                    from++;
                continue;
            }
            if (*from && *from != '\n')
                *to++ = *from++;
            if (*from == '\n') {
                *to++ = '\0';
                from++;
                break;
            }
            if (!*from)
                break;
        }
    }

    for (ctab_i = 0; ctab_i < ctab_free; ctab_i++) {
        if (strncmp(ctab[ctab_i], "cycbuff:", 8) == 0) {
            if (metacycbufffound) {
                warn("CNFS: all cycbuff entries should be before metacycbuff"
                     " entries");
                free(config);
                free(ctab);
                return false;
            }
            if (!CNFSparse_part_line(ctab[ctab_i] + 8)) {
                free(config);
                free(ctab);
                return false;
            }
        } else if (strncmp(ctab[ctab_i], "metacycbuff:", 12) == 0) {
            metacycbufffound = true;
            if (!CNFSparse_metapart_line(ctab[ctab_i] + 12)) {
                free(config);
                free(ctab);
                return false;
            }
        } else if (strncmp(ctab[ctab_i], "cycbuffupdate:", 14) == 0) {
            if (cycbuffupdatefound) {
                warn("CNFS: duplicate cycbuffupdate entries");
                free(config);
                free(ctab);
                return false;
            }
            cycbuffupdatefound = true;
            update = atoi(ctab[ctab_i] + 14);
            if (update < 0) {
                warn("CNFS: invalid cycbuffupdate");
                free(config);
                free(ctab);
                return false;
            }
            if (update == 0)
                metabuff_update = METACYCBUFF_UPDATE;
            else
                metabuff_update = update;
        } else if (strncmp(ctab[ctab_i], "refreshinterval:", 16) == 0) {
            if (refreshintervalfound) {
                warn("CNFS: duplicate refreshinterval entries");
                free(config);
                free(ctab);
                return false;
            }
            refreshintervalfound = true;
            refresh = atoi(ctab[ctab_i] + 16);
            if (refresh < 0) {
                warn("CNFS: invalid refreshinterval");
                free(config);
                free(ctab);
                return false;
            }
            if (refresh == 0)
                refresh_interval = REFRESH_INTERVAL;
            else
                refresh_interval = refresh;
        } else {
            warn("CNFS: bogus metacycbuff config line '%s' ignored",
                 ctab[ctab_i]);
        }
    }
    free(config);
    free(ctab);
    if (!CNFSparse_groups_line()) {
        return false;
    }
    if (cycbufftab == (CYCBUFF *) NULL) {
        warn("CNFS: zero cycbuffs defined");
        return false;
    }
    if (metacycbufftab == (METACYCBUFF *) NULL) {
        warn("CNFS: zero metacycbuffs defined");
        return false;
    }
    return true;
}

/* Figure out what page an address is in and flush those pages */
static void
cnfs_mapcntl(void *p, size_t length, int flags)
{
    char *start, *end;

    start = (char *) ((uintptr_t) p & ~(size_t) (pagesize - 1));
    end = (char *) (((uintptr_t) p + length + pagesize)
                    & ~(size_t) (pagesize - 1));
    if (flags == MS_INVALIDATE) {
        msync(start, end - start, flags);
    } else {
        static char *start2, *end2;

        /* Don't thrash the system with msync()s - keep the last value
         * and check each time, only if the pages which we should
         * flush change actually flush the previous ones.  Calling
         * cnfs_mapcntl(NULL, 0, MS_ASYNC) then flushes the final
         * piece. */
        if (start != start2 || end != end2) {
            if (start2 != NULL && end2 != NULL) {
                msync(start2, end2 - start2, flags);
            }
            start2 = start;
            end2 = end;
        }
    }
}

/*
** Bit arithmetic by brute force.
**
** FIXME: the code below is not endian-neutral!
*/

typedef unsigned long ULONG;

static int
CNFSUsedBlock(CYCBUFF *cycbuff, off_t offset, bool set_operation,
              bool setbitvalue)
{
    off_t blocknum;
    off_t longoffset;
    int bitoffset; /* From the 'left' side of the long */
    static int uninitialized = 1;
    static int longsize = sizeof(long);
    int i;
    ULONG bitlong, on, off, mask;
    static ULONG onarray[64], offarray[64];
    ULONG *where;

    if (uninitialized) {
        on = 1;
        off = on;
        off ^= ULONG_MAX;
        for (i = (longsize * 8) - 1; i >= 0; i--) {
            onarray[i] = on;
            offarray[i] = off;
            on <<= 1;
            off = on;
            off ^= ULONG_MAX;
        }
        uninitialized = 0;
    }

    /* We allow bit-setting under minartoffset, but it better be false */
    if ((offset < cycbuff->minartoffset && setbitvalue)
        || offset > cycbuff->len) {
        char bufoff[64], bufmin[64], bufmax[64];
        SMseterror(SMERR_INTERNAL, NULL);
        strlcpy(bufoff, CNFSofft2hex(offset, false), sizeof(bufoff));
        strlcpy(bufmin, CNFSofft2hex(cycbuff->minartoffset, false),
                sizeof(bufmin));
        strlcpy(bufmax, CNFSofft2hex(cycbuff->len, false), sizeof(bufmax));
        warn("CNFS: CNFSUsedBlock: invalid offset %s, min = %s, max = %s",
             bufoff, bufmin, bufmax);
        return 0;
    }
    if (offset % cycbuff->blksz != 0) {
        SMseterror(SMERR_INTERNAL, NULL);
        warn("CNFS: CNFSsetusedbitbyrp: offset %s not on %d-byte block"
             " boundary",
             CNFSofft2hex(offset, false), cycbuff->blksz);
        return 0;
    }
    blocknum = offset / cycbuff->blksz;
    longoffset = blocknum / (longsize * 8);
    bitoffset = blocknum % (longsize * 8);
    where = (ULONG *) cycbuff->bitfield + (CNFS_BEFOREBITF / longsize)
            + longoffset;
    bitlong = *where;
    if (set_operation) {
        if (setbitvalue) {
            mask = onarray[bitoffset];
            bitlong |= mask;
        } else {
            mask = offarray[bitoffset];
            bitlong &= mask;
        }
        *where = bitlong;
        if (innconf->nfswriter) {
            cnfs_mapcntl(where, sizeof *where, MS_ASYNC);
        }
        return 2; /* XXX Clean up return semantics */
    }
    /* It's a read operation */
    mask = onarray[bitoffset];

    /*
     * return bitlong & mask; doesn't work if sizeof(ulong) > sizeof(int)
     */
    if (bitlong & mask)
        return 1;
    else
        return 0;
}

static int
CNFSArtMayBeHere(CYCBUFF *cycbuff, off_t offset, uint32_t cycnum)
{
    static time_t lastupdate = 0;
    CYCBUFF *tmp;

    if (SMpreopen && !SMopenmode) {
        if ((time(NULL) - lastupdate)
            > refresh_interval) { /* XXX Changed to refresh every 30sec - cmo*/
            for (tmp = cycbufftab; tmp != (CYCBUFF *) NULL; tmp = tmp->next) {
                CNFSReadFreeAndCycle(tmp);
            }
            lastupdate = time(NULL);
        } else if (cycnum == cycbuff->cyclenum + 1) { /* rollover ? */
            CNFSReadFreeAndCycle(cycbuff);
        }
    }
    /* Basic range checks.
     * Our intent is to avoid a false negative response, *not* a false positive
     * response. */
    if (!(cycnum == cycbuff->cyclenum
          || (cycnum == cycbuff->cyclenum - 1 && offset >= cycbuff->free)
          || (cycnum + 1 == 0 && cycbuff->cyclenum == 2
              && offset > cycbuff->free))) {
        /* We've been overwritten */
        return 0;
    }
    return CNFSUsedBlock(cycbuff, offset, false, false);
}

bool
cnfs_init(SMATTRIBUTE *attr)
{
    METACYCBUFF *metacycbuff;
    CYCBUFF *cycbuff;

    if (attr == NULL) {
        warn("CNFS: attr is NULL");
        SMseterror(SMERR_INTERNAL, "attr is NULL");
        return false;
    }
    attr->selfexpire = true;
    attr->expensivestat = false;
    if (innconf == NULL) {
        if (!innconf_read(NULL)) {
            warn("CNFS: innconf_read failed");
            SMseterror(SMERR_INTERNAL, "ReadInnConf() failed");
            return false;
        }
    }
    if (pagesize == 0) {
        pagesize = getpagesize();
        if (pagesize == -1) {
            syswarn("CNFS: getpagesize failed");
            SMseterror(SMERR_INTERNAL, "getpagesize failed");
            pagesize = 0;
            return false;
        }
        if ((pagesize > CNFS_HDR_PAGESIZE) || (CNFS_HDR_PAGESIZE % pagesize)) {
            warn("CNFS: CNFS_HDR_PAGESIZE (%d) is not a multiple of"
                 " pagesize (%ld)",
                 CNFS_HDR_PAGESIZE, pagesize);
            SMseterror(SMERR_INTERNAL,
                       "CNFS_HDR_PAGESIZE not multiple of pagesize");
            return false;
        }
    }
    if (STORAGE_TOKEN_LENGTH < 16) {
        warn("CNFS: token length is less than 16 bytes");
        SMseterror(SMERR_TOKENSHORT, NULL);
        return false;
    }

    if (!CNFSread_config()) {
        CNFScleancycbuff();
        CNFScleanmetacycbuff();
        CNFScleanexpirerule();
        SMseterror(SMERR_INTERNAL, NULL);
        return false;
    }
    if (!CNFSinit_disks(NULL)) {
        CNFScleancycbuff();
        CNFScleanmetacycbuff();
        CNFScleanexpirerule();
        SMseterror(SMERR_INTERNAL, NULL);
        return false;
    }
    for (metacycbuff = metacycbufftab; metacycbuff != (METACYCBUFF *) NULL;
         metacycbuff = metacycbuff->next) {
        metacycbuff->memb_next = 0;
        metacycbuff->write_count = 0; /* Let's not forget this */
        if (metacycbuff->metamode == SEQUENTIAL)
            /* mark current cycbuff */
            if (CNFS_setcurrent(metacycbuff) == false) {
                CNFScleancycbuff();
                CNFScleanmetacycbuff();
                CNFScleanexpirerule();
                SMseterror(SMERR_INTERNAL, NULL);
                return false;
            }
    }
    if (!SMpreopen) {
        for (cycbuff = cycbufftab; cycbuff != (CYCBUFF *) NULL;
             cycbuff = cycbuff->next) {
            CNFSshutdowncycbuff(cycbuff);
        }
    }
    return true;
}

TOKEN
cnfs_store(const ARTHANDLE article, const STORAGECLASS class)
{
    TOKEN token;
    CYCBUFF *cycbuff = NULL;
    METACYCBUFF *metacycbuff = NULL;
    int i;
    static char buf[1024];
    static char alignbuf[CNFS_MAX_BLOCKSIZE];
    char *artcycbuffname;
    off_t artoffset, middle;
    uint32_t artcyclenum;
    CNFSARTHEADER cah;
    static struct iovec *iov;
    static int iovcnt;
    int tonextblock;
    CNFSEXPIRERULES *metaexprule;
    off_t left;
    size_t totlen;

    for (metaexprule = metaexprulestab;
         metaexprule != (CNFSEXPIRERULES *) NULL;
         metaexprule = metaexprule->next) {
        if (metaexprule->class == class)
            break;
    }
    if (metaexprule == (CNFSEXPIRERULES *) NULL) {
        SMseterror(SMERR_INTERNAL, "no rules match");
        warn("CNFS: no matches for group '%s'", buf);
        memset(&token, 0, sizeof(token));
        token.type = TOKEN_EMPTY;
        return token;
    }
    metacycbuff = metaexprule->dest;

    cycbuff = metacycbuff->members[metacycbuff->memb_next];
    if (cycbuff == NULL) {
        SMseterror(SMERR_INTERNAL, "no cycbuff found");
        warn("CNFS: no cycbuff found for %d", metacycbuff->memb_next);
        token.type = TOKEN_EMPTY;
        return token;
    } else if (!SMpreopen && !CNFSinit_disks(cycbuff)) {
        SMseterror(SMERR_INTERNAL, "cycbuff initialization fail");
        warn("CNFS: cycbuff '%s' initialization fail", cycbuff->name);
        token.type = TOKEN_EMPTY;
        return token;
    }

    /* cycbuff->free should have already been aligned by the last write, but
       realign it just to be sure. */
    tonextblock = cycbuff->blksz - (cycbuff->free & (cycbuff->blksz - 1));
    if (tonextblock != cycbuff->blksz)
        cycbuff->free += tonextblock;

    /* Article too big? */
    if (cycbuff->len - cycbuff->free < cycbuff->blksz + 1)
        left = 0;
    else
        left = cycbuff->len - cycbuff->free - cycbuff->blksz - 1;
    if ((off_t) article.len > left) {
        for (middle = cycbuff->free;
             middle < cycbuff->len - cycbuff->blksz - 1;
             middle += cycbuff->blksz) {
            CNFSUsedBlock(cycbuff, middle, true, false);
        }
        if (innconf->nfswriter) {
            cnfs_mapcntl(NULL, 0, MS_ASYNC);
        }
        cycbuff->free = cycbuff->minartoffset;
        cycbuff->cyclenum++;
        if (cycbuff->magicver <= 3) {
            if (cycbuff->cyclenum == 0)
                cycbuff->cyclenum += 2; /* cnfs_next() needs this */
        } else {
            if ((cycbuff->cyclenum & 0xFFFFFF) == 0) /* 24 bits max */
                cycbuff->cyclenum = 2; /* cnfs_next() needs this */
        }
        cycbuff->needflush = true;
        if (metacycbuff->metamode == INTERLEAVE) {
            CNFSflushhead(cycbuff); /* Flush, just for giggles */
            notice("CNFS: cycbuff %s rollover to cycle 0x%x... remain calm",
                   cycbuff->name, cycbuff->cyclenum);
        } else {
            /* SEQUENTIAL */
            cycbuff->currentbuff = false;
            CNFSflushhead(cycbuff); /* Flush, just for giggles */
            if (!SMpreopen)
                CNFSshutdowncycbuff(cycbuff);
            metacycbuff->memb_next =
                (metacycbuff->memb_next + 1) % metacycbuff->count;
            cycbuff = metacycbuff->members[metacycbuff->memb_next];
            notice("CNFS: metacycbuff %s cycbuff is moved to %s remain calm",
                   metacycbuff->name, cycbuff->name);
            if (!SMpreopen && !CNFSinit_disks(cycbuff)) {
                SMseterror(SMERR_INTERNAL, "cycbuff initialization fail");
                warn("CNFS: cycbuff '%s' initialization fail", cycbuff->name);
                token.type = TOKEN_EMPTY;
                return token;
            }
            cycbuff->currentbuff = true;
            cycbuff->needflush = true;
            CNFSflushhead(cycbuff); /* Flush, just for giggles */
        }
    }

    /* Ah, at least we know all three important data */
    artcycbuffname = cycbuff->name;
    artoffset = cycbuff->free;
    artcyclenum = cycbuff->cyclenum;

    memset(&cah, 0, sizeof(cah));
    cah.size = htonl(article.len);
    if (article.arrived == (time_t) 0)
        cah.arrived = htonl(time(NULL));
    else
        cah.arrived = htonl(article.arrived);
    cah.class = class;

    if (lseek(cycbuff->fd, artoffset, SEEK_SET) < 0) {
        SMseterror(SMERR_INTERNAL, "lseek failed");
        syswarn("CNFS: lseek failed for '%s' offset 0x%s", cycbuff->name,
                CNFSofft2hex(artoffset, false));
        token.type = TOKEN_EMPTY;
        if (!SMpreopen)
            CNFSshutdowncycbuff(cycbuff);
        return token;
    }
    if (iovcnt == 0) {
        iov = xmalloc((article.iovcnt + 2) * sizeof(struct iovec));
        iovcnt = article.iovcnt + 2;
    } else if (iovcnt < article.iovcnt + 2) {
        iov = xrealloc(iov, (article.iovcnt + 2) * sizeof(struct iovec));
        iovcnt = article.iovcnt + 2;
    }
    iov[0].iov_base = (char *) &cah;
    iov[0].iov_len = sizeof(cah);
    totlen = iov[0].iov_len;
    for (i = 1; i <= article.iovcnt; i++) {
        iov[i].iov_base = article.iov[i - 1].iov_base;
        iov[i].iov_len = article.iov[i - 1].iov_len;
        totlen += iov[i].iov_len;
    }
    if ((totlen & (cycbuff->blksz - 1)) != 0) {
        /* Want to xwritev an exact multiple of cycbuff->blksz */
        iov[i].iov_base = alignbuf;
        iov[i].iov_len = cycbuff->blksz - (totlen & (cycbuff->blksz - 1));
        totlen += iov[i].iov_len;
        i++;
    }
    if (xwritev(cycbuff->fd, iov, i) < 0) {
        SMseterror(SMERR_INTERNAL, "cnfs_store() xwritev() failed");
        syswarn("CNFS: cnfs_store xwritev failed for '%s' offset 0x%s",
                artcycbuffname, CNFSofft2hex(artoffset, false));
        token.type = TOKEN_EMPTY;
        if (!SMpreopen)
            CNFSshutdowncycbuff(cycbuff);
        return token;
    }
    cycbuff->needflush = true;

    /* Now that the article is written, advance the free pointer & flush */
    cycbuff->free += totlen;

    /*
    ** If cycbuff->free > cycbuff->len, don't worry.  The next cnfs_store()
    ** will detect the situation & wrap around correctly.
    */
    if (metacycbuff->metamode == INTERLEAVE)
        metacycbuff->memb_next =
            (metacycbuff->memb_next + 1) % metacycbuff->count;
    if (++metacycbuff->write_count % metabuff_update == 0) {
        for (i = 0; i < metacycbuff->count; i++) {
            CNFSflushhead(metacycbuff->members[i]);
        }
    }
    CNFSUsedBlock(cycbuff, artoffset, true, true);
    for (middle = artoffset + cycbuff->blksz; middle < cycbuff->free;
         middle += cycbuff->blksz) {
        CNFSUsedBlock(cycbuff, middle, true, false);
    }
    if (innconf->nfswriter) {
        cnfs_mapcntl(NULL, 0, MS_ASYNC);
    }
    if (!SMpreopen)
        CNFSshutdowncycbuff(cycbuff);
    return CNFSMakeToken(artcycbuffname, artoffset, cycbuff->blksz,
                         artcyclenum, class);
}

ARTHANDLE *
cnfs_retrieve(const TOKEN token, const RETRTYPE amount)
{
    char cycbuffname[9];
    off_t offset;
    uint32_t cycnum;
    uint32_t block;
    CYCBUFF *cycbuff;
    ARTHANDLE *art;
    CNFSARTHEADER cah;
    PRIV_CNFS *private;
    char *p;
    long pagefudge;
    off_t cycsize, mmapoffset;
    static TOKEN ret_token;
    static bool nomessage = false;
    int plusoffset = 0;

    if (token.type != TOKEN_CNFS) {
        SMseterror(SMERR_INTERNAL, NULL);
        return NULL;
    }
    if (!CNFSBreakToken(token, cycbuffname, &block, &cycnum)) {
        /* SMseterror() should have already been called */
        return NULL;
    }
    if ((cycbuff = CNFSgetcycbuffbyname(cycbuffname)) == NULL) {
        SMseterror(SMERR_NOENT, NULL);
        if (!nomessage) {
            warn("CNFS: cnfs_retrieve: token %s: bogus cycbuff name:"
                 " %s:0x%s:%u",
                 TokenToText(token), cycbuffname, CNFSofft2hex(block, false),
                 cycnum);
            nomessage = true;
        }
        return NULL;
    }
    if (!SMpreopen && !CNFSinit_disks(cycbuff)) {
        SMseterror(SMERR_INTERNAL, "cycbuff initialization fail");
        warn("CNFS: cycbuff '%s' initialization fail", cycbuff->name);
        return NULL;
    }
    offset = (off_t) block * cycbuff->blksz;
    if (!CNFSArtMayBeHere(cycbuff, offset, cycnum)) {
        SMseterror(SMERR_NOENT, NULL);
        if (!SMpreopen)
            CNFSshutdowncycbuff(cycbuff);
        return NULL;
    }

    art = xmalloc(sizeof(ARTHANDLE));
    art->type = TOKEN_CNFS;
    if (amount == RETR_STAT) {
        art->data = NULL;
        art->len = 0;
        art->private = NULL;
        ret_token = token;
        art->token = &ret_token;
        if (!SMpreopen)
            CNFSshutdowncycbuff(cycbuff);
        return art;
    }
    /*
    ** Because we don't know the length of the article (yet), we'll
    ** just mmap() a chunk of memory which is guaranteed to be larger
    ** than the largest article can be.
    ** XXX Because the max article size can be changed, we could get into hot
    ** XXX water here.  So, to be safe, we double MAX_ART_SIZE and add enough
    ** XXX extra for the pagesize fudge factor and CNFSARTHEADER structure.
    */
    if (pread(cycbuff->fd, &cah, sizeof(cah), offset) != sizeof(cah)) {
        SMseterror(SMERR_UNDEFINED, "read failed");
        syswarn("CNFS: could not read token %s %s:0x%s:%u", TokenToText(token),
                cycbuffname, CNFSofft2hex(offset, false), cycnum);
        free(art);
        if (!SMpreopen)
            CNFSshutdowncycbuff(cycbuff);
        return NULL;
    }
#ifdef OLD_CNFS
    if (cah.size == htonl(0x1234)
        && ntohl(cah.arrived) < time(NULL) - 10 * 365 * 24 * 3600) {
        oldCNFSARTHEADER cahh;
        *(CNFSARTHEADER *) &cahh = cah;
        if (pread(cycbuff->fd, ((char *) &cahh) + sizeof(CNFSARTHEADER),
                  sizeof(oldCNFSARTHEADER) - sizeof(CNFSARTHEADER),
                  offset + sizeof(cah))
            != sizeof(oldCNFSARTHEADER) - sizeof(CNFSARTHEADER)) {
            SMseterror(SMERR_UNDEFINED, "read2 failed");
            syswarn("CNFS: could not read2 token %s %s:0x%s:%u: %s",
                    TokenToText(token), cycbuffname,
                    CNFSofft2hex(offset, false), cycnum, strerror(errno));
            free(art);
            if (!SMpreopen)
                CNFSshutdowncycbuff(cycbuff);
            return NULL;
        }
        cah.size = cahh.size;
        cah.arrived = htonl(time(NULL));
        cah.class = 0;
        plusoffset = sizeof(oldCNFSARTHEADER) - sizeof(CNFSARTHEADER);
    }
#endif /* OLD_CNFS */
    if (offset > cycbuff->len - cycbuff->blksz - (off_t) ntohl(cah.size) - 1) {
        if (!SMpreopen) {
            SMseterror(SMERR_UNDEFINED, "CNFSARTHEADER size overflow");
            warn("CNFS: could not match article size token %s %s:0x%s:%u: %ld",
                 TokenToText(token), cycbuffname, CNFSofft2hex(offset, false),
                 cycnum, (long) ntohl(cah.size));
            free(art);
            CNFSshutdowncycbuff(cycbuff);
            return NULL;
        }
        CNFSReadFreeAndCycle(cycbuff);
        if (offset
            > cycbuff->len - cycbuff->blksz - (off_t) ntohl(cah.size) - 1) {
            SMseterror(SMERR_UNDEFINED, "CNFSARTHEADER size overflow");
            warn("CNFS: could not match article size token %s %s:0x%s:%u: %ld",
                 TokenToText(token), cycbuffname, CNFSofft2hex(offset, false),
                 cycnum, (long) ntohl(cah.size));
            free(art);
            return NULL;
        }
    }
    /* checking the bitmap to ensure cah.size is not broken was dropped */
    cycsize = ntohl(cah.size);
    if (innconf->cnfscheckfudgesize != 0 && innconf->maxartsize != 0
        && ((unsigned) cycsize
            > innconf->maxartsize + innconf->cnfscheckfudgesize)) {
        char buf1[24];
        strlcpy(buf1, CNFSofft2hex(cycbuff->free, false), sizeof(buf1));
        SMseterror(SMERR_UNDEFINED, "CNFSARTHEADER fudge size overflow");
        warn("CNFS: fudge size overflows bitmaps %s %s:0x%s: %ld",
             TokenToText(token), cycbuffname, buf1, (long) ntohl(cah.size));
        if (!SMpreopen)
            CNFSshutdowncycbuff(cycbuff);
        free(art);
        return NULL;
    }
    private = xmalloc(sizeof(PRIV_CNFS));
    art->private = (void *) private;
    art->arrived = ntohl(cah.arrived);
    offset += sizeof(cah) + plusoffset;
    if (innconf->articlemmap) {
        pagefudge = offset % pagesize;
        mmapoffset = offset - pagefudge;
        private->len = pagefudge + ntohl(cah.size);
        if ((private->base = mmap(NULL, private->len, PROT_READ, MAP_SHARED,
                                  cycbuff->fd, mmapoffset))
            == MAP_FAILED) {
            SMseterror(SMERR_UNDEFINED, "mmap failed");
            syswarn("CNFS: could not mmap token %s %s:0x%s:%u",
                    TokenToText(token), cycbuffname,
                    CNFSofft2hex(offset, false), cycnum);
            free(art->private);
            free(art);
            if (!SMpreopen)
                CNFSshutdowncycbuff(cycbuff);
            return NULL;
        }
        mmap_invalidate(private->base, private->len);
        if (amount == RETR_ALL)
            madvise(private->base, private->len, MADV_WILLNEED);
        else
            madvise(private->base, private->len, MADV_SEQUENTIAL);
    } else {
        private->base = xmalloc(ntohl(cah.size));
        pagefudge = 0;
        if (pread(cycbuff->fd, private->base, ntohl(cah.size), offset) < 0) {
            SMseterror(SMERR_UNDEFINED, "read failed");
            syswarn("CNFS: could not read token %s %s:0x%s:%u",
                    TokenToText(token), cycbuffname,
                    CNFSofft2hex(offset, false), cycnum);
            free(private->base);
            free(art->private);
            free(art);
            if (!SMpreopen)
                CNFSshutdowncycbuff(cycbuff);
            return NULL;
        }
    }
    ret_token = token;
    art->token = &ret_token;
    art->len = ntohl(cah.size);
    if (amount == RETR_ALL) {
        art->data =
            innconf->articlemmap ? private->base + pagefudge : private->base;
        if (!SMpreopen)
            CNFSshutdowncycbuff(cycbuff);
        return art;
    }
    if ((p = wire_findbody(innconf->articlemmap ? private->base + pagefudge
                                                : private->base,
                           art->len))
        == NULL) {
        SMseterror(SMERR_NOBODY, NULL);
        if (innconf->articlemmap)
            munmap(private->base, private->len);
        else
            free(private->base);
        free(art->private);
        free(art);
        if (!SMpreopen)
            CNFSshutdowncycbuff(cycbuff);
        return NULL;
    }
    if (amount == RETR_HEAD) {
        if (innconf->articlemmap) {
            art->data = private->base + pagefudge;
            art->len = p - private->base - pagefudge;
        } else {
            art->data = private->base;
            art->len = p - private->base;
        }
        /* Headers end just before the first empty line (\r\n). */
        art->len = art->len - 2;
        if (!SMpreopen)
            CNFSshutdowncycbuff(cycbuff);
        return art;
    }
    if (amount == RETR_BODY) {
        art->data = p;
        if (innconf->articlemmap)
            art->len = art->len - (p - private->base - pagefudge);
        else
            art->len = art->len - (p - private->base);
        if (!SMpreopen)
            CNFSshutdowncycbuff(cycbuff);
        return art;
    }
    SMseterror(SMERR_UNDEFINED, "Invalid retrieve request");
    if (innconf->articlemmap)
        munmap(private->base, private->len);
    else
        free(private->base);
    free(art->private);
    free(art);
    if (!SMpreopen)
        CNFSshutdowncycbuff(cycbuff);
    return NULL;
}

void
cnfs_freearticle(ARTHANDLE *article)
{
    PRIV_CNFS *private;

    if (!article)
        return;

    if (article->private) {
        private = (PRIV_CNFS *) article->private;
        if (innconf->articlemmap)
            munmap(private->base, private->len);
        else
            free(private->base);
        free(private);
    }
    free(article);
}

bool
cnfs_cancel(TOKEN token)
{
    char cycbuffname[9];
    off_t offset;
    uint32_t cycnum;
    uint32_t block;
    CYCBUFF *cycbuff;

    if (token.type != TOKEN_CNFS) {
        SMseterror(SMERR_INTERNAL, NULL);
        return false;
    }
    if (!CNFSBreakToken(token, cycbuffname, &block, &cycnum)) {
        SMseterror(SMERR_INTERNAL, NULL);
        /* SMseterror() should have already been called */
        return false;
    }
    if ((cycbuff = CNFSgetcycbuffbyname(cycbuffname)) == NULL) {
        SMseterror(SMERR_INTERNAL, "bogus cycbuff name");
        return false;
    }
    if (!SMpreopen && !CNFSinit_disks(cycbuff)) {
        SMseterror(SMERR_INTERNAL, "cycbuff initialization fail");
        warn("CNFS: cycbuff '%s' initialization fail", cycbuff->name);
        return false;
    }
    offset = (off_t) block * cycbuff->blksz;
    if (!(cycnum == cycbuff->cyclenum
          || (cycnum == cycbuff->cyclenum - 1 && offset > cycbuff->free)
          || (cycnum + 1 == 0 && cycbuff->cyclenum == 2
              && offset > cycbuff->free))) {
        SMseterror(SMERR_NOENT, NULL);
        if (!SMpreopen)
            CNFSshutdowncycbuff(cycbuff);
        return false;
    }
    if (CNFSUsedBlock(cycbuff, offset, false, false) == 0) {
        SMseterror(SMERR_NOENT, NULL);
        if (!SMpreopen)
            CNFSshutdowncycbuff(cycbuff);
        return false;
    }
    CNFSUsedBlock(cycbuff, offset, true, false);
    if (innconf->nfswriter) {
        cnfs_mapcntl(NULL, 0, MS_ASYNC);
    }
    if (!SMpreopen)
        CNFSshutdowncycbuff(cycbuff);
    return true;
}

ARTHANDLE *
cnfs_next(ARTHANDLE *article, const RETRTYPE amount)
{
    ARTHANDLE *art;
    CYCBUFF *cycbuff;
    PRIV_CNFS priv, *private;
    off_t middle = 0, limit;
    CNFSARTHEADER cah;
    off_t offset;
    long pagefudge, blockfudge;
    static TOKEN token;
    int tonextblock;
    off_t mmapoffset;
    char *p;
    int plusoffset = 0;

    if (article == NULL) {
        if ((cycbuff = cycbufftab) == NULL)
            return NULL;
        priv.offset = 0;
        priv.rollover = false;
        priv.len = 0;
        priv.base = NULL;
        priv.cycbuff = NULL;
    } else {
        priv = *(PRIV_CNFS *) article->private;
        free(article->private);
        free(article);
        if (innconf->articlemmap)
            munmap(priv.base, priv.len);
        else {
            /* In the case we return art->data = NULL, we
             * must not free an already stale pointer.
               -mibsoft@mibsoftware.com
             */
            if (priv.base) {
                free(priv.base);
                priv.base = 0;
            }
        }
        cycbuff = priv.cycbuff;
    }

    for (; cycbuff != (CYCBUFF *) NULL;
         cycbuff = cycbuff->next, priv.offset = 0) {

        if (!SMpreopen && !CNFSinit_disks(cycbuff)) {
            SMseterror(SMERR_INTERNAL, "cycbuff initialization fail");
            continue;
        }
        /* If a roll over is expected but offset is too far, go to the
         * beginning of the next cycbuff. */
        if (priv.rollover && priv.offset >= cycbuff->free) {
            priv.offset = 0;
            if (!SMpreopen)
                CNFSshutdowncycbuff(cycbuff);
            continue;
        }

        /* Set offset to the first article in the cycbuff after a roll over. */
        if (priv.offset == 0) {
            if (cycbuff->cyclenum == 1) {
                /* It is really the first cycle number as cnfs_store() sets the
                 * cycle to 2 when rolling over after the maximum number of
                 * cycles a cycbuff can go through.  Ask for a roll over in
                 * order to treat the articles up to cycbuff->free only. */
                priv.offset = cycbuff->minartoffset;
                priv.rollover = true;
            } else {
                /* Treat the oldest articles first (the ones of the previous
                 * cycle, that will soon be overwritten after cycbuff->free).
                 */
                priv.offset = cycbuff->free;
                priv.rollover = false;
            }
        }
        if (!priv.rollover) {
            /* Treat the articles until the end of the cycbuff is reached. */
            for (middle = priv.offset;
                 middle < cycbuff->len - cycbuff->blksz - 1;
                 middle += cycbuff->blksz) {
                if (CNFSUsedBlock(cycbuff, middle, false, false) != 0)
                    break;
            }
            /* The end of the cycbuff has been reached.  Ask for a roll over,
             * and go back to the beginning of this cycbuff to treat the
             * articles of the current cycle number. */
            if (middle >= cycbuff->len - cycbuff->blksz - 1) {
                priv.rollover = true;
                middle = cycbuff->minartoffset;
                /* If the beginning of cycbuff is the next place where to write
                 * an article, then it means that the cycbuff has fully
                 * wrapped, so the first article has already been walked
                 * through.  Only articles sharing the same cycle number are
                 * present in the cycbuff. */
                if (middle == cycbuff->free) {
                    if (!SMpreopen)
                        CNFSshutdowncycbuff(cycbuff);
                    continue;
                }
            }
            /* Treat the article (of the current cycle number). */
            break;
        } else {
            /* Treat the articles of the current cycle number, up to where
             * articles of the previous cycle number still are. */
            for (middle = priv.offset; middle < cycbuff->free;
                 middle += cycbuff->blksz) {
                if (CNFSUsedBlock(cycbuff, middle, false, false) != 0)
                    break;
            }
            /* Time to go to the next cycbuff? */
            if (middle >= cycbuff->free) {
                /* priv.offset will be set to 0 in the loop clause. */
                if (!SMpreopen)
                    CNFSshutdowncycbuff(cycbuff);
                continue;
            } else {
                /* Treat the article. */
                break;
            }
        }
    }
    if (cycbuff == (CYCBUFF *) NULL)
        return (ARTHANDLE *) NULL;

    offset = middle;
    if (pread(cycbuff->fd, &cah, sizeof(cah), offset) != sizeof(cah)) {
        if (!SMpreopen)
            CNFSshutdowncycbuff(cycbuff);
        return (ARTHANDLE *) NULL;
    }
#ifdef OLD_CNFS
    if (cah.size == htonl(0x1234)
        && ntohl(cah.arrived) < time(NULL) - 10 * 365 * 24 * 3600) {
        oldCNFSARTHEADER cahh;
        *(CNFSARTHEADER *) &cahh = cah;
        if (pread(cycbuff->fd, ((char *) &cahh) + sizeof(CNFSARTHEADER),
                  sizeof(oldCNFSARTHEADER) - sizeof(CNFSARTHEADER),
                  offset + sizeof(cah))
            != sizeof(oldCNFSARTHEADER) - sizeof(CNFSARTHEADER)) {
            if (!SMpreopen)
                CNFSshutdowncycbuff(cycbuff);
            return (ARTHANDLE *) NULL;
        }
        cah.size = cahh.size;
        cah.arrived = htonl(time(NULL));
        cah.class = 0;
        plusoffset = sizeof(oldCNFSARTHEADER) - sizeof(CNFSARTHEADER);
    }
#endif /* OLD_CNFS */
    art = xmalloc(sizeof(ARTHANDLE));
    private = xmalloc(sizeof(PRIV_CNFS));
    art->private = (void *) private;
    art->type = TOKEN_CNFS;
    *private = priv;
    private->cycbuff = cycbuff;
    private->offset = middle;
    if (cycbuff->len - cycbuff->free
        < (off_t) ntohl(cah.size) + cycbuff->blksz + 1) {
        private->offset += cycbuff->blksz;
        art->data = NULL;
        art->len = 0;
        art->token = NULL;
        if (!SMpreopen)
            CNFSshutdowncycbuff(cycbuff);
        return art;
    }
    /* check the bitmap to ensure cah.size is not broken */
    blockfudge = (sizeof(cah) + plusoffset + ntohl(cah.size)) % cycbuff->blksz;
    limit = private->offset + sizeof(cah) + plusoffset + ntohl(cah.size)
            - blockfudge + cycbuff->blksz;
    if (offset < cycbuff->free) {
        for (middle = offset + cycbuff->blksz;
             (middle < cycbuff->free) && (middle < limit);
             middle += cycbuff->blksz) {
            if (CNFSUsedBlock(cycbuff, middle, false, false) != 0)
                /* Bitmap set.  This article assumes to be broken */
                break;
        }
        if ((middle > cycbuff->free) || (middle != limit)) {
            private->offset = middle;
            art->data = NULL;
            art->len = 0;
            art->token = NULL;
            if (!SMpreopen)
                CNFSshutdowncycbuff(cycbuff);
            return art;
        }
    } else {
        for (middle = offset + cycbuff->blksz;
             (middle < cycbuff->len) && (middle < limit);
             middle += cycbuff->blksz) {
            if (CNFSUsedBlock(cycbuff, middle, false, false) != 0)
                /* Bitmap set.  This article assumes to be broken */
                break;
        }
        if ((middle >= cycbuff->len) || (middle != limit)) {
            private->offset = middle;
            art->data = NULL;
            art->len = 0;
            art->token = NULL;
            if (!SMpreopen)
                CNFSshutdowncycbuff(cycbuff);
            return art;
        }
    }
    if (innconf->cnfscheckfudgesize != 0 && innconf->maxartsize != 0
        && ((unsigned int) ntohl(cah.size)
            > innconf->maxartsize + innconf->cnfscheckfudgesize)) {
        art->data = NULL;
        art->len = 0;
        art->token = NULL;
        private->base = 0;
        if (!SMpreopen)
            CNFSshutdowncycbuff(cycbuff);
        return art;
    }

    private->offset += (off_t) ntohl(cah.size) + sizeof(cah) + plusoffset;
    tonextblock = cycbuff->blksz - (private->offset & (cycbuff->blksz - 1));
    private->offset += (off_t) tonextblock;
    art->arrived = ntohl(cah.arrived);
    /* Generate a token relative to the previous cycle number when offset is
     * where to begin overwritting files, or beyond. */
    token = CNFSMakeToken(cycbuff->name, offset, cycbuff->blksz,
                          (offset >= cycbuff->free) ? cycbuff->cyclenum - 1
                                                    : cycbuff->cyclenum,
                          cah.class);
    art->token = &token;
    offset += sizeof(cah) + plusoffset;
    if (innconf->articlemmap) {
        pagefudge = offset % pagesize;
        mmapoffset = offset - pagefudge;
        private->len = pagefudge + ntohl(cah.size);
        if ((private->base = mmap(0, private->len, PROT_READ, MAP_SHARED,
                                  cycbuff->fd, mmapoffset))
            == MAP_FAILED) {
            art->data = NULL;
            art->len = 0;
            art->token = NULL;
            if (!SMpreopen)
                CNFSshutdowncycbuff(cycbuff);
            return art;
        }
        mmap_invalidate(private->base, private->len);
        madvise(private->base, private->len, MADV_SEQUENTIAL);
    } else {
        private->base = xmalloc(ntohl(cah.size));
        pagefudge = 0;
        if (pread(cycbuff->fd, private->base, ntohl(cah.size), offset) < 0) {
            art->data = NULL;
            art->len = 0;
            art->token = NULL;
            if (!SMpreopen)
                CNFSshutdowncycbuff(cycbuff);
            free(private->base);
            private->base = 0;
            return art;
        }
    }
    art->len = ntohl(cah.size);
    if (amount == RETR_ALL) {
        art->data =
            innconf->articlemmap ? private->base + pagefudge : private->base;
        if (!SMpreopen)
            CNFSshutdowncycbuff(cycbuff);
        return art;
    }
    if ((p = wire_findbody(innconf->articlemmap ? private->base + pagefudge
                                                : private->base,
                           art->len))
        == NULL) {
        art->data = NULL;
        art->len = 0;
        art->token = NULL;
        if (!SMpreopen)
            CNFSshutdowncycbuff(cycbuff);
        return art;
    }
    if (amount == RETR_HEAD) {
        if (innconf->articlemmap) {
            art->data = private->base + pagefudge;
            art->len = p - private->base - pagefudge;
        } else {
            art->data = private->base;
            art->len = p - private->base;
        }
        /* Headers end just before the first empty line (\r\n). */
        art->len = art->len - 2;
        if (!SMpreopen)
            CNFSshutdowncycbuff(cycbuff);
        return art;
    }
    if (amount == RETR_BODY) {
        art->data = p;
        if (innconf->articlemmap)
            art->len = art->len - (p - private->base - pagefudge);
        else
            art->len = art->len - (p - private->base);
        if (!SMpreopen)
            CNFSshutdowncycbuff(cycbuff);
        return art;
    }
    art->data = NULL;
    art->len = 0;
    art->token = NULL;
    if (!SMpreopen)
        CNFSshutdowncycbuff(cycbuff);
    return art;
}

bool
cnfs_ctl(PROBETYPE type, TOKEN *token UNUSED, void *value)
{
    struct artngnum *ann;

    switch (type) {
    case SMARTNGNUM:
        if ((ann = (struct artngnum *) value) == NULL)
            return false;
        /* make SMprobe() call cnfs_retrieve() */
        ann->artnum = 0;
        return true;
    default:
        return false;
    }
}

bool
cnfs_flushcacheddata(FLUSHTYPE type)
{
    if (type == SM_ALL || type == SM_HEAD)
        CNFSflushallheads();
    return true;
}

void
cnfs_printfiles(FILE *file, TOKEN token, char **xref UNUSED,
                int ngroups UNUSED)
{
    fprintf(file, "%s\n", TokenToText(token));
}

void
cnfs_shutdown(void)
{
    CNFScleancycbuff();
    CNFScleanmetacycbuff();
    CNFScleanexpirerule();
}
