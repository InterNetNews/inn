#ifndef __STORAGE_H__
#define __STORAGE_H__

#define STORAGE_TOKEN_LENGTH 16

/* This is the type of an empty token.  Tokens with this type will be
   returned when errors occur */
#define TOKEN_EMPTY     255

typedef enum {RETR_ALL, RETR_HEAD, RETR_BODY, RETR_STAT} RETRTYPE;

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
} ARTHANDLE;

char *TokenToText(const TOKEN token);
TOKEN TextToToken(const char *text);
BOOL IsToken(const char *text);

BOOL      SMinit(void);
TOKEN     SMstore(const ARTHANDLE article);
ARTHANDLE *SMretrieve(const TOKEN token, const RETRTYPE amount);
ARTHANDLE *SMnext(const ARTHANDLE *article, const RETRTYPE amount);
void      SMfreearticle(ARTHANDLE *article);
BOOL      SMcancel(TOKEN token);
void      SMshutdown(void);

#endif
