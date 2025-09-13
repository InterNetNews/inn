/*
**  The Article class for innfeed.
**
**  Written by James Brister <brister@vix.com>
**
**  The implementation of the Article class. Articles are the abstraction for
**  the actual news articles. They are a reference counted object because they
**  need to be shared by multiple Host and Connection objects. When an Article
**  is created it verifies that the actual file exists. When a Connection
**  wants the article's contents for transmission the Article reads the data
**  off disk and returns a set of Buffer objects. The Article holds onto these
**  Buffers so that the next Connection that wants to transmit it won't have
**  to wait for a disk read to be done again.
*/

#include "portable/system.h"

#include "innfeed.h"
#include "portable/mmap.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <syslog.h>

#include "inn/libinn.h"
#include "inn/messages.h"
#include "inn/storage.h"

#include "article.h"
#include "buffer.h"
#include "endpoint.h"

#if defined(NDEBUG)
#    define VALIDATE_HASH_TABLE() (void(0))
#else
#    define VALIDATE_HASH_TABLE() hashValidateTable()
#endif

struct map_info_s {
    ARTHANDLE *arthandle;
    const void *mMapping;
    size_t size;
    int refCount;
    struct map_info_s *next;
};

typedef struct map_info_s *MapInfo;

struct article_s {
    int refCount;        /* the reference count on this article */
    char *fname;         /* the file name of the article */
    char *msgid;         /* the msgid of the article (INN tells us) */
    Buffer contents;     /* the buffer of the actual on disk stuff */
    Buffer *nntpBuffers; /* list of buffers for transmission */
    MapInfo mapInfo;     /* arthandle and mMapping */
    bool loggedMissing;  /* true if article is missing and we logged */
    bool articleOk;      /* true until we know otherwise. */
    bool inWireFormat;   /* true if ->contents is \r\n/dot-escaped */
};

struct hash_entry_s {
    struct hash_entry_s *next;
    struct hash_entry_s *prev;
    struct hash_entry_s *nextTime;
    struct hash_entry_s *prevTime;
    unsigned int hash;
    Article article;
};

typedef struct hash_entry_s *HashEntry;

/*
 * Private functions
 */

static Buffer artGetContents(Article article); /* Return the buffer that
                                                  fillContents() filled up. */

/* Log statistics on article memory usage. */
static void logArticleStats(TimeoutId, void *);

static bool fillContents(Article article); /* Read the article's bits
                                              off the disk. */

/* Append buffer B to the buffer array BUFFS. */
static void appendBuffer(Buffer b, Buffer **buffs, int *newSpot, int *curLen);

static bool prepareArticleForNNTP(Article article); /* Do the necessary
                                                       CR-LF stuff */

static bool artFreeContents(Article art); /* Tell the Article to release its
                                             contents buffer if possible. */

static void artUnmap(Article article); /* munmap an mmap()ed article */


/*
 * Hash table routine declarations.
 */

/* Returns a has value for the given string */
static unsigned int hashString(const char *string);

/* Locates the article with the given message ID, in the has table. */
static Article hashFindArticle(const char *msgid);

/* Puts the given article in the hash table. */
static void hashAddArticle(Article article);

/* Removes the given article from the has table */
static bool hashRemoveArticle(Article article);

/* Does some simple-minded hash table validation */
static void hashValidateTable(void);


/*
 * Private data
 */


static unsigned int
    missingArticleCount; /* Number of articles that were missing */

static bool logMissingArticles; /* if true then we log the details on a
                                   missing article. */

static unsigned int preparedBytes; /* The number of areticle bytes read in so
                               far of disk (will wrap around) */

static unsigned int preparedNewlines; /* The number of newlines found in all
                                  the preparedBytes */

static unsigned int avgCharsPerLine; /* The average number of characters per
                                 line over all articles. */

static bool rolledOver; /* true if preparedBytes wrapped around */

static unsigned int bytesInUse; /* count of article contents bytes in use--just
                            the amount read from the article files, not
                            all memory involved in. */

static unsigned int maxBytesInUse; /* the limit we want to stay under for total
                               bytes resident in memory dedicated to
                               article contents. */

static unsigned int
    articlesInUse; /* number of articles currently allocated. */

static unsigned int byteTotal; /* number of bytes for article contents
                           allocated totally since last log. */

static unsigned int
    articleTotal; /* number of articles alloced since last log. */

static TimeoutId articleStatsId; /* The timer callback id. */

static MapInfo mapInfo;

/*
 * Hash Table data
 */

static HashEntry *hashTable; /* the has table itself */
static HashEntry chronList;  /* chronologically ordered. Points at newest */

#define TABLE_SIZE        2048 /* MUST be a power of 2 */
#define HASH_MASK         (TABLE_SIZE - 1)
#define TABLE_ENTRY(hash) ((hash) & HASH_MASK)


/*******************************************************************/
/**                       PUBLIC FUNCTIONS                        **/
/*******************************************************************/

/* Create a new article object. First looks to see if one with the given
   message id already exists in the hash table and if so returns that
   (after incrementing the reference count). */
Article
newArticle(const char *filename, const char *msgid)
{
    Article newArt = NULL;

    TMRstart(TMR_NEWARTICLE);
    if (hashTable == NULL) { /* first-time through initialization. */
        int i;

        ASSERT((TABLE_SIZE & HASH_MASK) == 0);
        hashTable = xmalloc(sizeof(HashEntry) * TABLE_SIZE);

        addPointerFreedOnExit((char *) hashTable);

        for (i = 0; i < TABLE_SIZE; i++)
            hashTable[i] = NULL;

        if (ARTICLE_STATS_PERIOD > 0)
            articleStatsId =
                prepareSleep(logArticleStats, ARTICLE_STATS_PERIOD, 0);
    }

    /* now look for it in the hash table. We presume the disk file is still
       ok */
    if ((newArt = hashFindArticle(msgid)) == NULL) {
        newArt = xcalloc(1, sizeof(struct article_s));

        newArt->fname = xstrdup(filename);
        newArt->msgid = xstrdup(msgid);

        newArt->contents = NULL;
        newArt->mapInfo = NULL;
        newArt->refCount = 1;
        newArt->loggedMissing = false;
        newArt->articleOk = true;
        newArt->inWireFormat = false;

        d_printf(3, "Adding a new article(%p): %s\n", (void *) newArt, msgid);

        articlesInUse++;
        articleTotal++;

        hashAddArticle(newArt);
    } else {
        if (strcmp(filename, newArt->fname) != 0)
            warn("ME two filenames for same article: %s, %s", filename,
                 newArt->fname);

        newArt->refCount++;
        d_printf(2, "Reusing existing article for %s\n", msgid);
    }
    TMRstop(TMR_NEWARTICLE);
    return newArt;
}


/* Decrement the reference count on the article and free up its memory if
   the ref count gets to 0. */
void
delArticle(Article article)
{
    if (article == NULL)
        return;

    ASSERT(article->refCount > 0);

    if (--(article->refCount) == 0) {
        bool removed = hashRemoveArticle(article);

        ASSERT(removed == true);

        d_printf(2, "Cleaning up article (%p): %s\n", (void *) article,
                 article->msgid);

        if (article->contents != NULL) {
            if (article->mapInfo)
                artUnmap(article);
            else
                bytesInUse -= bufferDataSize(article->contents);

            if (article->nntpBuffers != NULL)
                freeBufferArray(article->nntpBuffers);

            delBuffer(article->contents);
        }

        articlesInUse--;

        free(article->fname);
        free(article->msgid);
        free(article);
    }

    VALIDATE_HASH_TABLE();
}


void
gPrintArticleInfo(FILE *fp, unsigned int indentAmt)
{
    char indent[INDENT_BUFFER_SIZE];
    unsigned int i;

    for (i = 0; i < MIN(INDENT_BUFFER_SIZE - 1, indentAmt); i++)
        indent[i] = ' ';
    indent[i] = '\0';

    fprintf(fp, "%sGlobal Article information : (count %u) {\n", indent,
            articlesInUse);

    fprintf(fp, "%s  missingArticleCount : %u\n", indent, missingArticleCount);
    fprintf(fp, "%s  logMissingArticles : %d\n", indent, logMissingArticles);
    fprintf(fp, "%s  preparedBytes : %u\n", indent, preparedBytes);
    fprintf(fp, "%s  preparedNewlines : %u\n", indent, preparedNewlines);
    fprintf(fp, "%s  avgCharsPerLine : %u\n", indent, avgCharsPerLine);
    fprintf(fp, "%s  rolledOver : %s\n", indent, boolToString(rolledOver));
    fprintf(fp, "%s  bytesInUse : %u\n", indent, bytesInUse);
    fprintf(fp, "%s  maxBytesInUse : %u\n", indent, maxBytesInUse);
    fprintf(fp, "%s  articlesInUse : %u\n", indent, articlesInUse);
    fprintf(fp, "%s  byteTotal : %u\n", indent, byteTotal);
    fprintf(fp, "%s  articleTotal : %u\n", indent, articleTotal);
    fprintf(fp, "%s  articleStatsId : %d\n", indent, articleStatsId);

    {
        HashEntry he;

        for (he = chronList; he != NULL; he = he->nextTime)
            printArticleInfo(he->article, fp, indentAmt + INDENT_INCR);
    }

    fprintf(fp, "%s}\n", indent);
}


void
printArticleInfo(Article art, FILE *fp, unsigned int indentAmt)
{
    Buffer *b;
    char indent[INDENT_BUFFER_SIZE];
    unsigned int i;

    for (i = 0; i < MIN(INDENT_BUFFER_SIZE - 1, indentAmt); i++)
        indent[i] = ' ';
    indent[i] = '\0';

    fprintf(fp, "%sArticle : %p {\n", indent, (void *) art);
    fprintf(fp, "%s    article ok : %s\n", indent,
            boolToString(art->articleOk));
    fprintf(fp, "%s    refcount : %d\n", indent, art->refCount);
    fprintf(fp, "%s    filename : %s\n", indent, art->fname);
    fprintf(fp, "%s    msgid : %s\n", indent, art->msgid);

    fprintf(fp, "%s    contents buffer : {\n", indent);
#if defined(INNFEED_DEBUG)
    printBufferInfo(art->contents, fp, indentAmt + INDENT_INCR);
#else
    fprintf(fp, "%s    %p\n", indent, (void *) art->contents);
#endif

    fprintf(fp, "%s    }\n", indent);

    fprintf(fp, "%s    nntp buffers : {\n", indent);
    for (b = art->nntpBuffers; b != NULL && *b != NULL; b++) {
#if defined(INNFEED_DEBUG)
        printBufferInfo(*b, fp, indentAmt + INDENT_INCR);
#else
        fprintf(fp, "%s    %p\n", indent, (void *) *b);
#endif
    }
    fprintf(fp, "%s    }\n", indent);

    fprintf(fp, "%s    logged missing : %s\n", indent,
            boolToString(art->loggedMissing));
    fprintf(fp, "%s}\n", indent);
}

/* return true if we have or are able to get the contents off the disk */
bool
artContentsOk(Article article)
{
    bool rval = false;

    if (prepareArticleForNNTP(article))
        rval = true;

    return rval;
}


/* bump reference count on the article. */
Article
artTakeRef(Article article)
{
    if (article != NULL)
        article->refCount++;

    return article;
}


/* return the filename of the article */
const char *
artFileName(Article article)
{
    if (article == NULL)
        return NULL;
    else
        return article->fname;
}


/* Get a NULL terminated array of Buffers that is ready for sending via NNTP */

Buffer *
artGetNntpBuffers(Article article)
{
    if (!prepareArticleForNNTP(article))
        return NULL;

    return dupBufferArray(article->nntpBuffers);
}


/* return the message id of the article */
const char *
artMsgId(Article article)
{
    return article->msgid;
}

/* return size of the article */
int
artSize(Article article)
{
    if (article == NULL || article->contents == NULL)
        return (int) 0;
    return (int) bufferDataSize(article->contents);
}


/* return how many NNTP-ready buffers the article contains */
unsigned int
artNntpBufferCount(Article article)
{
    if (!prepareArticleForNNTP(article))
        return 0;

    return bufferArrayLen(article->nntpBuffers);
}


/* if VAL is true then all missing articles will be logged. */
void
artLogMissingArticles(bool val)
{
    logMissingArticles = val;
}


/* set the limit we want to stay under. */
void
artSetMaxBytesInUse(unsigned int val)
{
    ASSERT(maxBytesInUse > 0); /* can only set one time. */
    ASSERT(val > 0);

    maxBytesInUse = val;
}


/**********************************************************************/
/**                            STATIC FUNCTIONS                      **/
/**********************************************************************/


/* return a single buffer that contains the disk image of the article (i.e.
   not fixed up for NNTP). */
static Buffer
artGetContents(Article article)
{
    Buffer rval = NULL;

    if (article->articleOk) {
        if (article->contents == NULL)
            fillContents(article);

        if (article->contents != NULL)
            rval = bufferTakeRef(article->contents);
    }

    return rval;
}

/* arthandle/mMapping needs to be refcounted since a buffer
   may exist referencing it after delArticle if the remote
   host sends the return status too soon (diablo, 439). */

static MapInfo
getMapInfo(ARTHANDLE *arthandle, const void *mMapping, size_t size)
{
    MapInfo m;

    for (m = mapInfo; m; m = m->next) {
        if (m->arthandle == arthandle && m->mMapping == mMapping) {
            m->refCount++;
            return m;
        }
    }

    m = xmalloc(sizeof(struct map_info_s));
    m->refCount = 1;
    m->arthandle = arthandle;
    m->mMapping = mMapping;
    m->size = size;
    m->next = mapInfo;
    mapInfo = m;

    return m;
}

static void
delMapInfo(void *vm)
{
    MapInfo i, prev;
    MapInfo m = (MapInfo) vm;

    if (m == NULL)
        return;

    if (--(m->refCount) > 0)
        return;

    if (m->arthandle)
        SMfreearticle(m->arthandle);
    else if (munmap(m->mMapping, m->size) < 0)
        syslog(LOG_NOTICE, "munmap article: %m");

    prev = NULL;
    for (i = mapInfo; i != m; i = i->next)
        prev = i;

    if (prev)
        prev->next = m->next;
    else
        mapInfo = m->next;

    free(m);
}

static void
artUnmap(Article article)
{

    delMapInfo(article->mapInfo);
    article->mapInfo = NULL;
}


static void
logArticleStats(TimeoutId id, void *data UNUSED)
{
    ASSERT(id == articleStatsId);

    notice("ME articles active %u bytes %u", articlesInUse, bytesInUse);
    notice("ME articles total %u bytes %u", articleTotal, byteTotal);

    byteTotal = 0;
    articleTotal = 0;

    articleStatsId = prepareSleep(logArticleStats, ARTICLE_STATS_PERIOD, 0);
}


/* do the actual read of the article off disk into a Buffer that is stored
   in the Article object. The Article will end up with its contents field
   having a buffer with the article data in it. This buffer may be
   holding a mmapped pointer, or it may be simply a regular buffer with
   the data read off disk into it. In the regular buffer case the
   contents may be copied around after reading to insert a carriage
   return before each newline. */

static bool
fillContents(Article article)
{
    int fd = -1;
    char *p;
    static bool maxLimitNotified;
    bool opened;
    size_t articlesize = 0;
    char *buffer = NULL;
    int amt = 0;
    size_t idx = 0, amtToRead;
    size_t newBufferSize;
    HashEntry h;
    ARTHANDLE *arthandle = NULL;
    const void *mMapping = NULL;

    ASSERT(article->contents == NULL);

    TMRstart(TMR_READART);
    if (maxBytesInUse == 0)
        maxBytesInUse = SOFT_ARTICLE_BYTE_LIMIT;

    if (avgCharsPerLine == 0)
        avgCharsPerLine = 75; /* roughly number of characters per line */

    if (IsToken(article->fname)) {
        opened =
            ((arthandle = SMretrieve(TextToToken(article->fname), RETR_ALL))
             != NULL)
                ? true
                : false;
        if (opened)
            articlesize = arthandle->len;
        else {
            if (SMerrno != SMERR_NOENT && SMerrno != SMERR_UNINIT) {
                syslog(LOG_ERR, "Could not retrieve %s: %s", article->fname,
                       SMerrorstr);
                article->articleOk = false;
                TMRstop(TMR_READART);
                return false;
            }
        }
    } else {
        struct stat sb;

        opened =
            ((fd = open(article->fname, O_RDONLY, 0)) >= 0) ? true : false;
        arthandle = NULL;
        if (opened) {
            if (fstat(fd, &sb) < 0) {
                article->articleOk = false;
                syswarn("ME oserr fstat %s", article->fname);
                TMRstop(TMR_READART);
                return false;
            }
            if (!S_ISREG(sb.st_mode)) {
                article->articleOk = false;
                warn("ME article file-type: %s", article->fname);
                TMRstop(TMR_READART);
                return false;
            }
            if (sb.st_size == 0) {
                article->articleOk = false;
                warn("ME article 0 bytes: %s", article->fname);
                TMRstop(TMR_READART);
                return false;
            }
            articlesize = sb.st_size;
        }
    }

    if (!opened) {
        article->articleOk = false;
        missingArticleCount++;

        if (logMissingArticles && !article->loggedMissing) {
            notice("ME article missing: %s, %s", article->msgid,
                   article->fname);
            article->loggedMissing = true;
        }
        TMRstop(TMR_READART);
        return false;
    }
    amtToRead = articlesize;
    newBufferSize = articlesize;

    if (arthandle || useMMap) {
        if (arthandle)
            mMapping = arthandle->data;
        else
            mMapping = mmap(NULL, articlesize, PROT_READ, MAP_SHARED, fd, 0);

        if (mMapping == MAP_FAILED) {
            /* dunno, but revert to plain reading */
            mMapping = NULL;
            syswarn("ME mmap failure %s (%s)", article->fname,
                    strerror(errno));
        } else {
            article->contents =
                newBufferByCharP(mMapping, articlesize, articlesize);
            article->mapInfo = getMapInfo(arthandle, mMapping, articlesize);
            article->mapInfo->refCount++; /* one more for the buffer */
            bufferSetDeletedCbk(article->contents, delMapInfo,
                                article->mapInfo);

            buffer = bufferBase(article->contents);
            if ((p = strchr(buffer, '\n')) == NULL) {
                article->articleOk = false;
                delBuffer(article->contents);
                article->contents = NULL;
                warn("ME munged article %s", article->fname);
            } else {
                if (p > buffer && p[-1] == '\r') {
                    article->inWireFormat = true;
                } else {
                    /* we need to copy the contents into a buffer below */
                    delBuffer(article->contents);
                    article->contents = NULL;
                }
            }
        }
    }

    if (article->contents == NULL && article->articleOk) {
        /* an estimate to give some room for nntpPrepareBuffer to use. */
        newBufferSize *= (size_t) (1.0 + (1.0 / avgCharsPerLine));
        newBufferSize++;

        /* if we're going over the limit try to free up some older article's
           contents. */
        if (amtToRead + bytesInUse > maxBytesInUse) {
            for (h = chronList; h != NULL; h = h->nextTime) {
                if (artFreeContents(h->article))
                    if (amtToRead + bytesInUse <= maxBytesInUse)
                        break;
            }
        }

        /* we we couldn't get below, then log it (one time only) */
        if ((amtToRead + bytesInUse) > maxBytesInUse
            && maxLimitNotified == false) {
            maxLimitNotified = true;
            notice("ME exceeding maximum article byte limit: %u (max),"
                   " %lu (cur)",
                   maxBytesInUse, (unsigned long) (amtToRead + bytesInUse));
        }

        if ((article->contents = newBuffer(newBufferSize)) == NULL)
            amtToRead = 0;
        else {
            buffer = bufferBase(article->contents);
            bytesInUse += articlesize;
            byteTotal += articlesize;
        }

        if (mMapping && buffer != NULL) {
            memcpy(buffer, mMapping, articlesize);
            artUnmap(article);
            amtToRead = 0;
        }

        while (amtToRead > 0) {
            if ((amt = read(fd, buffer + idx, amtToRead)) <= 0) {
                syswarn("ME article read error: %s", article->fname);
                bytesInUse -= articlesize;
                byteTotal -= articlesize;
                amtToRead = 0;

                delBuffer(article->contents);
                article->contents = NULL;
            } else {
                idx += amt;
                amtToRead -= amt;
            }
        }

        if (article->contents != NULL) {
            bufferSetDataSize(article->contents, articlesize);

            if ((p = strchr(buffer, '\n')) == NULL) {
                article->articleOk = false;
                warn("ME munged article %s", article->fname);
            } else if (p > buffer && p[-1] == '\r') {
                article->inWireFormat = true;
            } else {
                if (nntpPrepareBuffer(article->contents)) {
                    size_t diff =
                        (bufferDataSize(article->contents) - articlesize);

                    if (((unsigned int) UINT_MAX) - diff <= preparedBytes) {
                        d_printf(2, "Newline ratio so far: %02.2f\n",
                                 ((double) preparedBytes / preparedNewlines));
                        notice("ME newline to file size ratio: %0.2f (%u/%u)",
                               ((double) preparedBytes) / preparedNewlines,
                               preparedBytes, preparedNewlines);
                        preparedBytes = 0;
                        preparedNewlines = 0;
                        rolledOver = true;
                    }

                    preparedBytes += articlesize;
                    preparedNewlines += diff;
                    bytesInUse += diff;
                    byteTotal += diff;

                    if (preparedBytes > (1024 * 1024)) {
                        avgCharsPerLine = preparedBytes / preparedNewlines;
                        avgCharsPerLine++;
                    }
                    article->inWireFormat = true;
                } else {
                    warn("ME internal failed to prepare buffer for NNTP");
                    bytesInUse -= articlesize;
                    byteTotal -= articlesize;

                    delBuffer(article->contents);
                    article->contents = NULL;
                }
            }
        }
    }


    /* If we're not using storage api, we should close a valid file descriptor
     */
    if (!arthandle && (fd >= 0))
        close(fd);

    TMRstop(TMR_READART);
    return (article->contents != NULL ? true : false);
}


/* stick the buffer B into the Buffer array pointed at by BUFFS *BUFFS is
   reallocated if necessary. NEWSPOT points at the index where B should be
   put (presumably the end). CURLEN points at the length of the BUFFS and
   it will get updated if BUFFS is reallocated. */
static void
appendBuffer(Buffer b, Buffer **buffs, int *newSpot, int *curLen)
{
    if (*newSpot == *curLen) {
        *curLen += 10;
        *buffs = xrealloc(*buffs, sizeof(Buffer) * *curLen);
    }
    (*buffs)[(*newSpot)++] = b;
}


/* Takes the articles contents buffer and overlays a set of new buffers on
   top of it. These buffers insert the required carriage return and dot
   characters as needed */
static bool
prepareArticleForNNTP(Article article)
{
    static Buffer dotBuffer;
    Buffer *nntpBuffs = NULL;
    int buffLen = 0;
    int buffIdx = 0;
    char *start, *end;
    Buffer contents;

    contents = artGetContents(article); /* returns a reference */

    TMRstart(TMR_PREPART);
    if (contents == NULL) {
        TMRstop(TMR_PREPART);
        return false;
    } else if (article->nntpBuffers != NULL) {
        delBuffer(contents);
        TMRstop(TMR_PREPART);
        return true; /* already done */
    }

    if (dotBuffer == NULL) {
        dotBuffer = newBufferByCharP(".\r\n", 3, 3);
    }

    /* overlay a set of buffers on top of the articles contents buffer. This
       is a real speed loss at the moment, so by default it's disabled (by
       calling artBitFiddleContents(true) in main(). */
    if (article->inWireFormat == false) {
        end = bufferBase(contents);
        do {
            start = end;

            while (*end && *end != '\n')
                end++;

            appendBuffer(newBufferByCharP(start, (size_t) (end - start),
                                          (size_t) (end - start)),
                         &nntpBuffs, &buffIdx, &buffLen);

            if (*end != '\0')
                end++;

        } while (*end != '\0');

        appendBuffer(bufferTakeRef(dotBuffer), &nntpBuffs, &buffIdx, &buffLen);
        appendBuffer(NULL, &nntpBuffs, &buffIdx, &buffLen);
    } else {
        /* we already fixed the contents up when we read in the article */
        nntpBuffs = xmalloc(sizeof(Buffer) * 3);
        nntpBuffs[0] =
            newBufferByCharP(bufferBase(contents), bufferDataSize(contents),
                             bufferDataSize(contents));
        if (article->mapInfo) {
            article->mapInfo->refCount++;
            bufferSetDeletedCbk(nntpBuffs[0], delMapInfo, article->mapInfo);
        }
        nntpBuffs[1] = NULL;
    }


    delBuffer(contents); /* the article is still holding a reference */
    article->nntpBuffers = nntpBuffs;
    TMRstop(TMR_PREPART);
    return true;
}


/* free the contents of the buffers if article is the only thing holding a
   reference. Returns true if it could, false if it couldn't */
static bool
artFreeContents(Article art)
{
    if (art->contents == NULL)
        return false;

    if (art->nntpBuffers != NULL) {
        if (bufferRefCount(art->nntpBuffers[0]) > 1)
            return false;
        else {
            freeBufferArray(art->nntpBuffers);
            art->nntpBuffers = NULL;
        }
    }

    ASSERT(bufferRefCount(art->contents) == 1);

    if (art->mapInfo)
        artUnmap(art);
    else
        bytesInUse -= bufferDataSize(art->contents);

    delBuffer(art->contents);

    art->contents = NULL;

    return true;
}


/**********************************************************************/
/*         Private hash table and routines for storing articles       */
/**********************************************************************/


/* Hash function lifted from perl 5 */
static unsigned int
hashString(const char *string)
{
    unsigned int i;

    for (i = 0; string && *string; string++)
        i = 33 * i + (u_char) *string;

    return i;
}


/* find the article in the has table and return it. */
static Article
hashFindArticle(const char *msgid)
{
    unsigned int hash = hashString(msgid);
    HashEntry h;

    for (h = hashTable[TABLE_ENTRY(hash)]; h != NULL; h = h->next)
        if (hash == h->hash && strcmp(msgid, h->article->msgid) == 0)
            break;

    return (h == NULL ? NULL : h->article);
}


/* add the article to the hash table. */
static void
hashAddArticle(Article article)
{
    unsigned int hash = hashString(article->msgid);
    HashEntry h;
    HashEntry ne;

    h = hashTable[TABLE_ENTRY(hash)];

    ne = xmalloc(sizeof(struct hash_entry_s));

    ne->article = article;
    ne->hash = hash;
    ne->next = hashTable[TABLE_ENTRY(hash)];
    ne->prev = NULL;

    if (h != NULL)
        h->prev = ne;

    hashTable[TABLE_ENTRY(hash)] = ne;

    ne->nextTime = chronList;
    ne->prevTime = NULL;

    if (chronList != NULL)
        chronList->prevTime = ne;

    chronList = ne;
}


/* remove the article from the hash table and chronological list.
   Does not delete the article itself. */
static bool
hashRemoveArticle(Article article)
{
    unsigned int hash = hashString(article->msgid);
    HashEntry h;

    for (h = hashTable[TABLE_ENTRY(hash)]; h != NULL; h = h->next)
        if (hash == h->hash && strcmp(article->msgid, h->article->msgid) == 0)
            break;

    if (h == NULL)
        return false;

    if (h == hashTable[TABLE_ENTRY(hash)]) {
        hashTable[TABLE_ENTRY(hash)] = h->next;
        if (h->next != NULL)
            h->next->prev = NULL;
    } else {
        h->prev->next = h->next;
        if (h->next != NULL)
            h->next->prev = h->prev;
    }

    if (chronList == h) {
        chronList = h->nextTime;
        if (chronList != NULL)
            chronList->prevTime = NULL;
    } else {
        h->prevTime->nextTime = h->nextTime;
        if (h->nextTime != NULL)
            h->nextTime->prevTime = h->prevTime;
    }

    free(h);

    return true;
}

#define HASH_VALIDATE_BUCKET_COUNT 1 /* hash buckets to check per call */

static void
hashValidateTable(void)
{
    static int hbn = 0;
    int i;
    HashEntry he;

#if !defined(NDEBUG)
    for (i = 0; i < HASH_VALIDATE_BUCKET_COUNT; i++) {
        for (he = hashTable[hbn]; he != NULL; he = he->next)
            ASSERT(he->article->refCount > 0);
        if (++hbn >= TABLE_SIZE)
            hbn = 0;
    }
#endif
}
