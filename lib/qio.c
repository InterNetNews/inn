/*  $Revision$
**
**  Quick I/O package -- optimized for reading through a file.
*/
#include <stdio.h>
#include <sys/types.h>
#include "configdata.h"
#include "clibrary.h"
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>
#include <errno.h>
#include "libinn.h"
#include "qio.h"
#include "macros.h"


/*
**  Open a quick file from a descriptor.
*/
QIOSTATE *
QIOfdopen(fd, size)
    int		fd;
    int		size;
{
    QIOSTATE	*qp;
#if	defined(DO_HAVE_ST_BLKSIZE)
    struct stat	Sb;
#endif	/* defined(DO_HAVE_ST_BLKSIZE) */

    qp = NEW(QIOSTATE, 1);
    qp->flag = QIO_ok;
    qp->fd = fd;
#if	defined(DO_HAVE_ST_BLKSIZE)
    if (size == 0)
	size = fstat(fd, &Sb) >= 0 ? (int)Sb.st_blksize : QIO_BUFFER;
#else
    if (size == 0)
	size = QIO_BUFFER;
#endif	/* defined(DO_HAVE_ST_BLKSIZE) */
    qp->Size = size;
    qp->Buffer = NEW(char, size);
    qp->Count = 0;
    qp->Start = qp->Buffer;
    qp->End = qp->Buffer;
    qp->WireFormat = -1;

    return qp;
}


/*
**  Open a file for reading.
*/
QIOSTATE *
QIOopen(name, size)
    char	*name;
    int		size;
{
    int		fd;

    /* Open the file, read in the first chunk. */
    if ((fd = open(name, O_RDONLY)) < 0)
	return NULL;
    return QIOfdopen(fd, size);
}


/*
**  Close an open stream.
*/
void
QIOclose(qp)
    QIOSTATE	*qp;
{
    (void)close(qp->fd);
    DISPOSE(qp->Buffer);
    DISPOSE(qp);
}


/*
**  Rewind an open stream.
*/
int
QIOrewind(qp)
    QIOSTATE	*qp;
{
    int		i;

    if (lseek(qp->fd, (OFFSET_T) 0, SEEK_SET) == -1)
	return -1;
    i = read(qp->fd, qp->Buffer, (SIZE_T)qp->Size);
    if (i < 0)
	return i;
    qp->Count = i;
    qp->Start = qp->Buffer;
    qp->End = &qp->Buffer[i];
    return 0;
}


/*
**  Get the next line from the input.
*/
char *
QIOread(qp)
    QIOSTATE	*qp;
{
    register char       *p;
    register char       *q;
    char                *save;
    int                 i;

    while (TRUE) {

        /* Read from buffer if there is any data there. */
        if (qp->End > qp->Start) {

            /* Find the newline. */
            p = memchr((POINTER)qp->Start, '\n', (SIZE_T)(qp->End - qp->Start));
            if (p != NULL) {
                if ((qp->WireFormat == 1) && (*(p-1) == '\r')) {
                    *(p-1) = '\0';
                    qp->Length = p - qp->Start - 1;
                } else {
                    *p = '\0';
                    qp->Length = p - qp->Start;
                }
                save = qp->Start;
                qp->Start = p + 1;
                qp->flag = QIO_ok;
                if (qp->WireFormat && (*save == '.')) {
		    qp->Length--;
                    if (qp->Length)
                        return save + 1;
                    return NULL;
                }
                return save;
            }

            /* Not there; move unread part down to start of buffer. */
            for (p = qp->Buffer, q = qp->Start; q < qp->End; )
                *p++ = *q++;
        }
        else
            p = qp->Buffer;

        /* Read data, reset the pointers. */
        i = read(qp->fd, p, (SIZE_T)(&qp->Buffer[qp->Size] - p));
        if (i < 0) {
            qp->flag = QIO_error;
            return NULL;
        }
        if (i == 0) {
            qp->flag = QIO_ok;
            return NULL;
        }
        qp->Count += i;
        qp->Start = qp->Buffer;
        qp->End = &p[i];

        /* Now try to find it. */
        p = memchr((POINTER)qp->Start, '\n', (SIZE_T)(qp->End - qp->Start));
        if (p != NULL) {
            if ((qp->WireFormat == -1) && (p != qp->Start) && (*(p-1) == '\r'))
                qp->WireFormat = 1;
            else if (qp->WireFormat == -1)
                qp->WireFormat = 0;
            if ((qp->WireFormat == 1) && (*(p-1) == '\r')) {
                *(p-1) = '\0';
                qp->Length = p - qp->Start - 1;
            } else {
                *p = '\0';
                qp->Length = p - qp->Start;
            }
            save = qp->Start;
            qp->Start = p + 1;
            qp->flag = QIO_ok;
            if (qp->WireFormat && (*save == '.')) {
		qp->Length--;
                if (qp->Length)
                    return save + 1;
                return NULL;
            }
            return save;
        }

        if ((qp->End - qp->Start) >= qp->Size) {
            /* Still not there and buffer is full -- line is too long. */
            qp->flag = QIO_long;
            qp->Start = qp->End;
            return NULL;
        }
        /* otherwise, try to read more (in case we're reading from a pipe or
 	   something that won't always give us a full block at once.) */
    }
}



#if	defined(TEST)
int
main(ac, av)
    int		ac;
    char	*av[];
{
    QIOSTATE	*h;
    char	*p;
    long	where;

    h = QIOopen(ac > 1 ? av[1] : "/usr/lib/news/history", 0);
    if (h == NULL) {
	perror("Can't open file");
	exit(1);
    }

    where = QIOtell(h);
    while ((p = QIOread(h)) != NULL) {
	printf("%5ld %3d %s\n", where, QIOlength(h), p);
	where = QIOtell(h);
    }
    if (QIOtoolong(h)) {
	fprintf(stderr, "Line too line at %ld\n", QIOtell(h));
	exit(1);
    }
    if (QIOerror(h)) {
	perror("Can't read");
	exit(1);
    }
    QIOclose(h);
    exit(0);
    /* NOTREACHED */
}
#endif	/* defined(TEST) */
