/*  $Revision$
**
**  Data structures, functions and cetera used for config file parsing.
*/

#ifdef __cplusplus
extern "C" {
#endif

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

extern CONFFILE *CONFfopen(char*);
extern void CONFfclose(CONFFILE*);

extern CONFTOKEN *CONFgettoken(CONFTOKEN*, CONFFILE*);

#ifdef __cplusplus
}
#endif
