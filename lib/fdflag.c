/* $Id$
 *
 * Set or clear file descriptor flags.
 *
 * Simple functions (wrappers around fcntl) to set or clear file descriptor
 * flags like close-on-exec or nonblocking I/O.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Copyright 2008, 2011, 2013
 *     The Board of Trustees of the Leland Stanford Junior University
 * Copyright (c) 2004, 2005, 2006
 *     by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1991, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001,
 *     2002, 2003 by The Internet Software Consortium and Rich Salz
 *
 * This code is derived from software contributed to the Internet Software
 * Consortium by Rich Salz.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "config.h"
#include "clibrary.h"
#include <errno.h>

#include "inn/fdflag.h"
#include "inn/libinn.h"

#ifdef _WIN32
# include <winsock2.h>
#else
# include <fcntl.h>
# ifndef O_NONBLOCK
#  include <sys/ioctl.h>
#  if HAVE_SYS_FILIO_H
#   include <sys/filio.h>
#  endif
# endif
#endif


/*
 * Set a file to close-on-exec (or clear that setting if the flag is false),
 * returning true on success and false on failure.
 *
 * One is supposed to retrieve the flags, add FD_CLOEXEC, and then set them,
 * although I've never seen a system with any flags other than close-on-exec.
 * Do it right anyway; it's not that expensive.
 *
 * Stub this out on Windows, where it's not supported (at least currently by
 * this utility library).
 */
#ifdef _WIN32
bool
fdflag_close_exec(int fd UNUSED, bool flag UNUSED)
{
    return false;
}
#else
bool
fdflag_close_exec(int fd, bool flag)
{
    int oflag, mode;

    oflag = fcntl(fd, F_GETFD, 0);
    if (oflag < 0)
        return false;
    mode = flag ? (oflag | FD_CLOEXEC) : (oflag & ~FD_CLOEXEC);
    return (fcntl(fd, F_SETFD, mode) == 0);
}
#endif


/*
 * Set a file descriptor to nonblocking (or clear the nonblocking flag if flag
 * is false), returning true on success and false on failure.
 *
 * For Windows, be aware that this will only work for sockets.  For UNIX, you
 * can pass a non-socket in and it will do the right thing, since UNIX doesn't
 * distinguish, but Windows will not allow that.  Thankfully, there's rarely
 * any need to set non-sockets non-blocking.
 *
 * For UNIX, always use O_NONBLOCK; O_NDELAY is not the same thing
 * historically.  The semantics of O_NDELAY are that if the read would block,
 * it returns 0 instead.  This is indistinguishable from an end of file
 * condition.  POSIX added O_NONBLOCK, which requires read to return -1 and
 * set errno to EAGAIN, which is what we want.
 *
 * FNDELAY (4.3BSD) originally did the correct thing, although it has a
 * different incompatibility (affecting all users of a socket rather than just
 * a file descriptor and returning EWOULDBLOCK instead of EAGAIN) that we
 * probably don't care about.  Using it is probably safe, but BSD should also
 * have the ioctl, and at least on Solaris FNDELAY does the same thing as
 * O_NDELAY, not O_NONBLOCK.  So if we don't have O_NONBLOCK, fall back to the
 * ioctl instead.
 *
 * Reference:  Stevens, Advanced Unix Programming, pg. 364.
 *
 * Note that O_NONBLOCK is known not to work on earlier versions of ULTRIX,
 * SunOS, and AIX, possibly not setting the socket nonblocking at all, despite
 * the fact that they do define it.  It works in later SunOS and, current AIX,
 * however, and a 1999-10-25 survey of current operating systems failed to
 * turn up any that didn't handle it correctly (as required by POSIX), while
 * HP-UX 11.00 did use the broken return-zero semantics of O_NDELAY (most
 * other operating systems surveyed treated O_NDELAY as synonymous with
 * O_NONBLOCK).  Accordingly, we currently unconditionally use O_NONBLOCK.  If
 * this causes too many problems, an autoconf test may be required.
 */
#if defined(_WIN32)
bool
fdflag_nonblocking(socket_type fd, bool flag)
{
    u_long mode;

    mode = flag ? 1 : 0;
    return (ioctlsocket(fd, FIONBIO, &mode) == 0);
}
#elif defined(O_NONBLOCK)
bool
fdflag_nonblocking(socket_type fd, bool flag)
{
    int mode;

    mode = fcntl(fd, F_GETFL, 0);
    if (mode < 0)
        return false;
    mode = (flag ? (mode | O_NONBLOCK) : (mode & ~O_NONBLOCK));
    return (fcntl(fd, F_SETFL, mode) == 0);
}
#else /* !O_NONBLOCK */
bool
fdflag_nonblocking(socket_type fd, bool flag)
{
    int state;

    state = flag ? 1 : 0;
    return (ioctl(fd, FIONBIO, &state) == 0);
}
#endif /* !O_NONBLOCK */
