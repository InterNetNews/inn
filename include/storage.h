#ifndef __STORAGE_H__
#define __STORAGE_H__
#include <sys/time.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define STORAGE_TOKEN_LENGTH 16

/* This is the type of an empty token.  Tokens with this type will be
   returned when errors occur */
#define TOKEN_EMPTY     255

typedef enum {RETR_ALL, RETR_HEAD, RETR_BODY, RETR_STAT} RETRTYPE;
typedef enum {SM_RDWR, SM_PREOPEN} SMSETUP;

#define NUM_STORAGE_CLASSES 256
typedef unsigned char STORAGECLASS;
typedef unsigned char STORAGETYPE;

typedef struct {
    STORAGETYPE         type;
    STORAGECLASS        class;
    char                token[STORAGE_TOKEN_LENGTH];
} TOKEN;

typedef struct {
    unsigned char       type;       /* For retrieved articles this indicates the method
				       that retrieved it */
    char                *data;      /* This is where the requested data starts */
    int                 len;        /* This is the length of the requested data */
    unsigned char       nextmethod; /* This is the next method to try when
				       iterating over the spool */
    void                *private;   /* This is a pointer to method specific data */
    time_t              arrived;    /* This is the time when article arrived */
    TOKEN               *token;     /* This is a pointer to TOKEN for article */
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

extern int              SMerrno;
extern char             *SMerrorstr;

typedef enum {SELFEXPIRE, SMARTNGNUM} PROBETYPE;
typedef enum {SM_ALL, SM_HEAD, SM_CANCELEDART} FLUSHTYPE;

struct artngnum {
    char	*groupname;
    ARTNUM	artnum;
};

char *TokenToText(const TOKEN token);
TOKEN TextToToken(const char *text);
BOOL IsToken(const char *text);

BOOL      SMsetup(SMSETUP type, void *value);
BOOL      SMinit(void);
TOKEN     SMstore(const ARTHANDLE article);
ARTHANDLE *SMretrieve(const TOKEN token, const RETRTYPE amount);
ARTHANDLE *SMnext(const ARTHANDLE *article, const RETRTYPE amount);
void      SMfreearticle(ARTHANDLE *article);
BOOL      SMcancel(TOKEN token);
BOOL      SMprobe(PROBETYPE type, TOKEN *token, void *value);
BOOL      SMflushcacheddata(FLUSHTYPE type);
void      SMshutdown(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */
    
#endif
