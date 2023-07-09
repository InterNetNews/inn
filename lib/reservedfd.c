/*
**  Routines to reserve file descriptors for stdio.
**
**  On platforms that have a stdio limitation, such as 64-bit Solaris versions
**  prior to 11.0, all 32-bit Solaris versions and 32-bit applications running
**  on 64-bit Solaris, these functions allow reserving low-numbered file
**  descriptors so that they could be re-used.
**  Without this mechanism, some essential files like the history file may not
**  have any file descriptor left when being reopened after a closure for some
**  operations, and stdio would fail.
**
*/

#include "portable/system.h"

#include <fcntl.h>

#include "inn/libinn.h"


static FILE **Reserved_fd = NULL;
static int Maxfd = -1;

/*
 * Reserve the given number of file descriptors for future use.  They all read
 * /dev/null, until Fopen() is called on them.
 * If fdnum is 0 or negative, all reserved file descriptors are closed.
 * Otherwise, the given number of file descriptors is reserved.  If this
 * function is called again with a higher fdnum value, it reserves more file
 * descriptors; with a lower fdnum value, it closes the supernumerary reserved
 * file descriptors.
 *
 * Return true when all the file descriptors have been successfully reserved,
 * and false otherwise, in which case all the reserved file descriptors are
 * closed, even if some of them were previously in use.
 */
bool
fdreserve(int fdnum)
{
    static int allocated = 0;
    int i, start = allocated;

    if (fdnum <= 0) {
        if (Reserved_fd != NULL) {
            for (i = 0; i < Maxfd; i++) {
                fclose(Reserved_fd[i]);
            }
            free(Reserved_fd);
            Reserved_fd = NULL;
        }
        Maxfd = -1;
        allocated = 0;
        return true;
    }

    /* Allocate Reserved_fd or extend it when needed. */
    if (Reserved_fd == NULL) {
        Reserved_fd = xmalloc(fdnum * sizeof(FILE *));
        allocated = fdnum;
    } else {
        if (allocated < fdnum) {
            Reserved_fd = xrealloc(Reserved_fd, fdnum * sizeof(FILE *));
            allocated = fdnum;
        } else if (Maxfd > fdnum) {
            for (i = fdnum; i < Maxfd; i++) {
                fclose(Reserved_fd[i]);
            }
        }
    }

    for (i = start; i < fdnum; i++) {
        if (((Reserved_fd[i] = fopen("/dev/null", "r")) == NULL)) {
            /* In case a file descriptor cannot be reserved,
             * close all of them. */
            for (--i; i >= 0; i--)
                fclose(Reserved_fd[i]);
            free(Reserved_fd);
            Reserved_fd = NULL;
            allocated = 0;
            Maxfd = -1;
            return false;
        }
    }

    Maxfd = fdnum;
    return true;
}

/*
 * Open the file name in the given mode, using the file descriptor in position
 * fdindex in the reserved file descriptors.
 *
 * If fdindex is lower than the number of reserved file descriptors, Fopen()
 * uses the corresponding one.  Otherwise, it just calls fopen() without
 * re-using a reserved file descriptor.
 *
 * Return a pointer to a FILE struct, or NULL on failure.
 */
FILE *
Fopen(const char *name, const char *mode, int fdindex)
{
    FILE *nfp;

    if (name == NULL || *name == '\0')
        return NULL;

    if (fdindex < 0 || fdindex > Maxfd || Reserved_fd[fdindex] == NULL)
        return fopen(name, mode);

    if ((nfp = freopen(name, mode, Reserved_fd[fdindex])) == NULL) {
        Reserved_fd[fdindex] = freopen("/dev/null", "r", Reserved_fd[fdindex]);
        return NULL;
    }

    return (Reserved_fd[fdindex] = nfp);
}

/*
 * If the file descriptor used for fp is reserved, Fclose() keeps it to read
 * /dev/null.  Otherwise, it just calls fclose() without keeping it.
 *
 * Return 0 on success.  Any other value (like EOF) is a failure.
 */
int
Fclose(FILE *fp)
{
    int i;

    if (fp == NULL)
        return 0;

    for (i = 0; i < Maxfd; i++) {
        if (Reserved_fd[i] == fp)
            break;
    }

    if (i >= Maxfd)
        return fclose(fp);

    Reserved_fd[i] = freopen("/dev/null", "r", Reserved_fd[i]);
    return 0;
}
