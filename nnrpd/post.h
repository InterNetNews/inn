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
    char *       Value;
} HEADER;

#define HDR(_x) (Table[(_x)].Value)
