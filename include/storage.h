/*  $Id$
**
**  Here be declarations related to the storage subsystem.
*/

#ifndef __STORAGE_H__
#define __STORAGE_H__

/* We've probably already included this; only include it if we need it. */
#ifndef __CONFIG_H__
# include "config.h"
#endif

#include <stdio.h>
#include <sys/types.h>

#define STORAGE_TOKEN_LENGTH 16

/* This is the type of an empty token.  Tokens with this type will be
   returned when errors occur */
#define TOKEN_EMPTY     255

typedef enum {RETR_ALL, RETR_HEAD, RETR_BODY, RETR_STAT} RETRTYPE;
typedef enum {SM_RDWR, SM_PREOPEN} SMSETUP;

#define NUM_STORAGE_CLASSES 256
typedef unsigned char STORAGECLASS;
typedef unsigned char STORAGETYPE;

typedef struct token {
    STORAGETYPE         type;
    STORAGECLASS        class;
    char                token[STORAGE_TOKEN_LENGTH];
} TOKEN;

typedef struct {
  unsigned char  type;       /* Method that retrieved the article */
  char           *data;      /* Where the requested data starts */
  struct iovec   *iov;       /* writev() style vector */
  int            iovcnt;     /* writev() style count */
  int            len;        /* Length of the requested data */
  unsigned char  nextmethod; /* Next method to try when iterating over the
				spool */
  void           *private;   /* A pointer to method specific data */
  time_t         arrived;    /* The time when the article arrived */
  time_t         expires;    /* The time when the article will be expired */
  char           *groups;    /* Where Newsgroups header starts */
  int            groupslen;  /* Length of Newsgroups header */
  TOKEN          *token;     /* A pointer to the article's TOKEN */
} ARTHANDLE;

#define SMERR_NOERROR          0
#define SMERR_INTERNAL         1
#define SMERR_UNDEFINED        2
#define SMERR_NOENT            3
#define SMERR_TOKENSHORT       4
#define SMERR_NOBODY           5
#define SMERR_UNINIT           6
#define SMERR_CONFIG           7
#define SMERR_BADHANDLE        8
#define SMERR_BADTOKEN         9
#define SMERR_NOMATCH         10

extern int              SMerrno;
extern char             *SMerrorstr;

typedef enum {SELFEXPIRE, SMARTNGNUM, EXPENSIVESTAT} PROBETYPE;
typedef enum {SM_ALL, SM_HEAD, SM_CANCELEDART} FLUSHTYPE;

struct artngnum {
    char	*groupname;
    ARTNUM	artnum;
};

BEGIN_DECLS

char *      TokenToText(const TOKEN token);
TOKEN       TextToToken(const char *text);
bool        IsToken(const char *text);
char *      ToWireFmt(const char *article, int len, int *newlen);
char *      FromWireFmt(const char *article, int len, int *newlen);
            
bool        SMsetup(SMSETUP type, void *value);
bool        SMinit(void);
TOKEN       SMstore(const ARTHANDLE article);
ARTHANDLE * SMretrieve(const TOKEN token, const RETRTYPE amount);
ARTHANDLE * SMnext(const ARTHANDLE *article, const RETRTYPE amount);
void        SMfreearticle(ARTHANDLE *article);
bool        SMcancel(TOKEN token);
bool        SMprobe(PROBETYPE type, TOKEN *token, void *value);
bool        SMflushcacheddata(FLUSHTYPE type);
void        SMprintfiles(FILE *file, TOKEN token, char **xref, int ngroups);
void        SMshutdown(void);

END_DECLS
    
#endif
