/*  $Revision$
**
**  Data structures, functions and cetera used for config file parsing.
*/

typedef struct {
    FILE *f;
    char *buf;
    int sbuf;
    int lineno;
} CONFFILE;

typedef struct {
    int type;
#define CONFstring	-1
    char *name;
} CONFTOKEN;

extern char CONFerror[];

extern CONFFILE *CONFfopen(char*);
extern void CONFfclose(CONFFILE*);

extern CONFTOKEN *CONFgettoken(CONFTOKEN*, CONFFILE*);
