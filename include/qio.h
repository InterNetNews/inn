/*  $Revision$
**
**  Quick I/O package -- optimized for reading through a file.
*/

#include <storage.h>

#ifdef __cplusplus
extern "C" {   
#endif /* __cplusplus */

/*
**  State for a quick open file.
*/
typedef struct _QIOSTATE {
    int		fd;
    int		Size;
    int		flag;
    int		Length;
    char	*Buffer;
    char	*End;
    char	*Start;
    long	Count;          /* Number of bytes read so far */
    int         WireFormat;
    ARTHANDLE   *handle;        /* Storage API handle for the open article */
} QIOSTATE;

    /* A reasonable buffersize to use. */
#define QIO_BUFFER	8192

    /* Values for QIOstate.flag */
#define QIO_ok		0
#define QIO_error	1
#define QIO_long	2

#define QIOerror(qp)		((qp)->flag > 0)
#define QIOtoolong(qp)		((qp)->flag == QIO_long)
#define QIOtell(qp)		((qp)->Count - ((qp)->End - (qp)->Start))
#define QIOlength(qp)		((qp)->Length)
#define QIOfileno(qp)		((qp)->fd)

extern QIOSTATE	*QIOopen(const char *name);
extern QIOSTATE	*QIOfdopen(const int fd);
extern char	*QIOread(QIOSTATE *qp);
extern void	QIOclose(QIOSTATE *qp);
extern int	QIOrewind(QIOSTATE *qp);

#ifdef __cplusplus
}
#endif /* __cplusplus */
