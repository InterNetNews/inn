/*  $Revision$
**
**  Net News Reading Protocol server.
*/
typedef enum _HEADERTYPE {
    HTobs,
    HTreq,
    HTstd
} HEADERTYPE;

typedef struct _HEADER {
    STRING	Name;
    BOOL	CanSet;
    HEADERTYPE	Type;
    int		Size;
    char	*Value;	/* just after ':' in header */
    char	*Body;	/* where actual body begins */
    int		Len;	/* body length excluding trailing white spaces */
} HEADER;

#define HDR(_x) (Table[(_x)].Body)
#define HDR_SET(_x, _y) Table[(_x)].Body = Table[(_x)].Value = _y ; Table[(_x)].Len = strlen(_y)
