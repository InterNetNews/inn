/*  $Id$
**
**  Interface to innfeed's configuration file parser.
**
**  Written by James Brister <brister@vix.com>
*/

#if ! defined ( configfile_h__ )
#define configfile_h__

/* pointer to function taking void-star param and returning int. */
typedef int (*PFIVP)(void *) ;

typedef enum { intval, charval, boolval, realval, stringval, scopeval } tag ;
  
typedef struct _scope {
    struct _value *me ;
    char *scope_type ;
    int value_count ;
    int value_idx ;
    struct _value **values ;
    struct _scope *parent ;
} scope ;

typedef struct _value {
    char *name ;
    struct _scope *myscope ;
    tag type ;
    union {
        char *charp_val ;
        char char_val ;
        double real_val ;
        int bool_val ;
        long int_val ;
        struct _scope *scope_val ;
    } v ;
} value ;

extern scope *topScope ;
extern char *errbuff ;

int isWord (scope *s, const char *name, int inherit) ;
int isName (scope *s, const char *name, int inherit) ;

int getReal (scope *s, const char *name, double *rval, int inherit) ;
int getInteger (scope *s, const char *name, long *rval, int inherit) ;
int getBool (scope *s, const char *name, int *rval, int inherit) ;
int getString (scope *s, const char *name, char **rval, int inherit) ;
int getWord (scope *s, const char *name, char **rval, int inherit) ;

void freeScopeTree (scope *s) ;
char *addInteger (scope *s, const char *name, long val) ;
char *addChar (scope *s, const char *name, char val) ;
char *addBoolean (scope *s, const char *name, int val) ;
char *addName (scope *s, const char *name, char *val) ;
char *addWord (scope *s, const char *name, char *val) ;
char *addReal (scope *s, const char *name, double val) ;
char *addString (scope *s, const char *name, const char *val) ;
scope *findScope (scope *s, const char *name, int mustExist) ;
value *findValue (scope *s, const char *name, int inherit) ;
value *findPeer (const char *name) ;
value *getNextPeer (int *cookie) ;
void configAddLoadCallback (PFIVP func,void *arg) ;
void configRemoveLoadCallback (PFIVP func) ;
int readConfig (const char *file, FILE *errorDest, int justCheck, int dump) ;
int buildPeerTable (FILE *fp, scope *currScope);
void configCleanup (void) ;

#define ARTICLE_TIMEOUT "article-timeout"
#define BACKLOG_LIMIT "backlog-limit"
#define INITIAL_CONNECTIONS "initial-connections"
#define IP_NAME "ip-name"
#define MAX_CONNECTIONS "max-connections"
#define MAX_QUEUE_SIZE "max-queue-size"
#define NO_CHECK_HIGH "no-check-high"
#define NO_CHECK_LOW "no-check-low"
#define PORT_NUMBER "port-number"
#define RESP_TIMEOUT "response-timeout"
#define STREAMING "streaming"
#define DROP_DEFERRED "drop-deferred"

#define ISPEER(V) (ISSCOPE(V) && strcmp ((V)->v.scope_val->scope_type,"peer") == 0)
#define ISSCOPE(V) (V->type == scopeval)

#define INHERIT 1
#define NO_INHERIT 0

/* Interface between lexer and parser. */
int yylex (void) ; 

#endif /* configfile_h__ */
