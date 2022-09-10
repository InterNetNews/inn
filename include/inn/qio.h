/*
**  Quick I/O package.
**
**  The interface to the Quick I/O package, optimized for reading through
**  files line by line.  This package uses internal buffering like stdio,
**  but is even more aggressive about its buffering.
*/

#ifndef INN_QIO_H
#define INN_QIO_H 1

#include "inn/macros.h"

/* This is the maximum line length that can be read by a QIO operation.  Since
   QIO is used by some overview manipulation tools, it must therefore be
   larger than the longest overview line INN supports. */
#define QIO_BUFFERSIZE (32 * 1024)

BEGIN_DECLS

/*
**  State for a quick open file, equivalent to FILE for stdio.  All callers
**  should treat this structure as opaque and instead use the functions and
**  macros defined below.
*/
enum QIOflag {
    QIO_ok,
    QIO_error,
    QIO_long
};

typedef struct {
    int _fd;
    size_t _length; /* Length of the current string. */
    size_t _size;   /* Size of the internal buffer. */
    char *_buffer;
    char *_start; /* Start of the unread data. */
    char *_end;   /* End of the available data. */
    off_t _count; /* Number of bytes read so far. */
    enum QIOflag _flag;
} QIOSTATE;

#define QIOerror(qp)   ((qp)->_flag != QIO_ok)
#define QIOtoolong(qp) ((qp)->_flag == QIO_long)
#define QIOfileno(qp)  ((qp)->_fd)
#define QIOlength(qp)  ((qp)->_length)
#define QIOtell(qp)    ((qp)->_count - ((qp)->_end - (qp)->_start))

extern QIOSTATE *QIOopen(const char *name);
extern QIOSTATE *QIOfdopen(int fd);
extern char *QIOread(QIOSTATE *qp);
extern void QIOclose(QIOSTATE *qp);
extern int QIOrewind(QIOSTATE *qp);

END_DECLS

#endif /* !INN_QIO_H */
