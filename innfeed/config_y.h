#define PEER 257
#define GROUP 258
#define IVAL 259
#define RVAL 260
#define NAME 261
#define XSTRING 262
#define SCOPE 263
#define COLON 264
#define LBRACE 265
#define RBRACE 266
#define TRUEBVAL 267
#define FALSEBVAL 268
#define CHAR 269
#define WORD 270
#define IP_ADDRESS 271
typedef union{
    scope *scp ;
    value *val ;
    char *name ;
    int integer ;
    double real ;
    char *string ;
    char chr ;
} YYSTYPE;
extern YYSTYPE yylval;
