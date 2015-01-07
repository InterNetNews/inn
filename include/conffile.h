/*  $Id$
**
**  Data structures, functions and cetera used for config file parsing.
*/

#include <inn/defines.h>

BEGIN_DECLS

typedef struct {
    FILE *f;
    char *buf;
    unsigned int sbuf;
    int lineno;
    int array_len;
    char **array;
    char *filename;
} CONFFILE;

typedef struct {
    int type;
#define CONFstring	-1
    char *name;
} CONFTOKEN;

extern char CONFerror[];

extern CONFFILE *CONFfopen(const char *);
extern void CONFfclose(CONFFILE *);

extern CONFTOKEN *CONFgettoken(CONFTOKEN *, CONFFILE *);

END_DECLS
