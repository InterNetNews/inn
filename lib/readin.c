#include "portable/system.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "inn/libinn.h"


/*
**  Read a big amount, looping until it is all done.  Return true if
**  successful.
*/
int
xread(int fd, char *p, off_t i)
{
    ssize_t count;
    size_t request;

    if (i < 0) {
        errno = EINVAL;
        return -1;
    }

    for (; i; p += count, i -= count) {
        request =
            (uintmax_t) i > (uintmax_t) SSIZE_MAX ? SSIZE_MAX : (size_t) i;
        do {
            count = read(fd, p, request);
        } while (count == -1 && errno == EINTR);
        if (count < 0)
            return -1;
        if (count == 0) {
            errno = EIO;
            return -1;
        }
    }
    return 0;
}


/*
**  Read an already-open file into memory.
*/
char *
ReadInDescriptor(int fd, struct stat *Sbp)
{
    struct stat mystat;
    char *p;
    size_t size;
    int oerrno;

    if (Sbp == NULL)
        Sbp = &mystat;

    /* Get the size, and enough memory. */
    if (fstat(fd, Sbp) < 0)
        return NULL;
    if (Sbp->st_size < 0 || (uintmax_t) Sbp->st_size >= SIZE_MAX) {
        errno = EOVERFLOW;
        return NULL;
    }
    size = (size_t) Sbp->st_size;
    p = xmalloc(size + 1);

    /* Slurp, slurp. */
    if (xread(fd, p, Sbp->st_size) < 0) {
        oerrno = errno;
        free(p);
        errno = oerrno;
        return NULL;
    }

    /* Terminate the string; terminate the routine. */
    p[size] = '\0';
    return p;
}


/*
**  Read a file into allocated memory.  Optionally fill in the stat(2) data.
**  Return a pointer to the file contents, or NULL on error.
*/
char *
ReadInFile(const char *name, struct stat *Sbp)
{
    char *p;
    int fd, oerrno;

    if ((fd = open(name, O_RDONLY)) < 0)
        return NULL;

    p = ReadInDescriptor(fd, Sbp);
    if (p == NULL) {
        oerrno = errno;
        close(fd);
        errno = oerrno;
    } else {
        close(fd);
    }
    return p;
}
