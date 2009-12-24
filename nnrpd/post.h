/*  $Id$
**
**  NetNews Reading Protocol server.
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
    char *       Value; /* Just after ':' in header. */
    char *       Body;  /* Where actual body begins. */
    int          Len;   /* Body length excluding trailing white spaces. */
} HEADER;

#define HDR(_x) (Table[(_x)].Body)
#define HDR_SET(_x, _y)                         \
    do {                                        \
        Table[(_x)].Body = _y;                  \
        Table[(_x)].Value = _y;                 \
        if (_y == NULL) {                       \
            Table[(_x)].Len = 0;                \
        } else {                                \
            Table[(_x)].Len = strlen(_y);       \
        }                                       \
    } while (0)
#define HDR_CLEAR(_x)                           \
    do {                                        \
        Table[(_x)].Body = NULL;                \
        Table[(_x)].Value = NULL;               \
        Table[(_x)].Len = 0;                    \
    } while (0)

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
#define HDR__DISTRIBUTION    13
#define HDR__EXPIRES	     14
#define HDR__MESSAGEID	     15
#define HDR__INJECTION_DATE  26
#define HDR__INJECTION_INFO  27
#define HDR__CC		     36
#define HDR__BCC	     37
#define HDR__TO		     38
