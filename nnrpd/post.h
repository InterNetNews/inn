/*  $Id$
**
**  Net News Reading Protocol server.
*/

typedef enum _HEADERTYPE {
    HTobs,
    HTreq,
    HTstd
} HEADERTYPE;

typedef struct _HEADER {
    const char * Name;
    bool         CanSet;
    HEADERTYPE   Type;
    int          Size;
    char *       Value; /* just after ':' in header */
    char *       Body;  /* where actual body begins */
    int          Len;   /* body length excluding trailing white spaces */
} HEADER;

#define HDR(_x) (Table[(_x)].Body)
#define HDR_SET(_x, _y) \
    Table[(_x)].Body = Table[(_x)].Value = _y; \
    if (_y == NULL) { \
	Table[(_x)].Len = 0; \
    } else { \
	Table[(_x)].Len = strlen(_y); \
    }

#define HDR__PATH	      0
#define HDR__FROM	      1
#define HDR__NEWSGROUPS	      2
#define HDR__SUBJECT	      3
#define HDR__CONTROL	      4
#define HDR__FOLLOWUPTO	      6
#define HDR__DATE	      7
#define HDR__ORGANIZATION     8
#define HDR__LINES	      9
#define HDR__SENDER	     10
#define HDR__APPROVED	     11
#define HDR__DISTRIBUTION    12
#define HDR__EXPIRES	     13
#define HDR__MESSAGEID	     14
#define HDR__NNTPPOSTINGHOST 17
#define HDR__XTRACE          21
#define HDR__XCOMPLAINTSTO   22
#define HDR__NNTPPOSTINGDATE 23
#define HDR__CC		     32
#define HDR__BCC	     33
#define HDR__TO		     34
