typedef union{
    scope *scp ;
    value *val ;
    char *name ;
    int integer ;
    double real ;
    char *string ;
    char chr ;
} YYSTYPE;
#define	PEER	258
#define	GROUP	259
#define	IVAL	260
#define	RVAL	261
#define	NAME	262
#define	XSTRING	263
#define	SCOPE	264
#define	COLON	265
#define	LBRACE	266
#define	RBRACE	267
#define	TRUEBVAL	268
#define	FALSEBVAL	269
#define	CHAR	270
#define	WORD	271
#define	IP_ADDRESS	272


extern YYSTYPE yylval;
