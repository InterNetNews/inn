/*  $Revision$
**
**  Net News Reading Protocol server.
*/

/*
**  Header types.
*/
typedef enum _ARTHEADERTYPE {
    HTreq,                      /* Drop article if this is missing      */
    HTobs,                      /* Delete this header if found          */
    HTstd,                      /* Standard optional header             */
    HTsav			/* Save header, but delete from article */
} ARTHEADERTYPE;

/*
**  Entry in the header table.
*/
typedef struct _ARTHEADER {
    STRING              Name;
    ARTHEADERTYPE       Type;
    int                 Size;                   /* Length of Name       */
    char                *Value;
    int                 Length;                 /* Length of Value      */
    int                 Found;
    BOOL                Allocated;
} ARTHEADER;

/*
**  A way to index into the header table.
*/
#define HDR(_x)         (ARTheaders[(_x)].Value)

