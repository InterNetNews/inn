/* $Id$
 *
 * fdflag test suite.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2008, 2009
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#define LIBTEST_NEW_FORMAT 1

#include "config.h"
#include "clibrary.h"
#include "portable/socket.h"

#include <errno.h>
#include <sys/wait.h>

#include "tap/basic.h"
#include "inn/fdflag.h"


int
main(void)
{
    int master, data, out1, out2;
    socklen_t size;
    ssize_t status;
    struct sockaddr_in sin;
    pid_t child;
    char buffer[] = "D";

    plan(8);

    /* Parent will create the socket first to get the port number. */
    memset(&sin, '\0', sizeof(sin));
    sin.sin_family = AF_INET;
    master = socket(AF_INET, SOCK_STREAM, 0);
    if (master == -1)
        sysbail("socket creation failed");
    if (bind(master, (struct sockaddr *) &sin, sizeof(sin)) < 0)
        sysbail("bind failed");
    size = sizeof(sin);
    if (getsockname(master, (struct sockaddr *) &sin, &size) < 0)
        sysbail("getsockname failed");
    if (listen(master, 1) < 0)
        sysbail("listen failed");

    /* Duplicate standard output to test close-on-exec. */
    out1 = 8;
    out2 = 9;
    if (dup2(fileno(stdout), out1) < 0)
        sysbail("cannot dup stdout to fd 8");
    if (dup2(fileno(stdout), out2) < 0)
        sysbail("cannot dup stdout to fd 9");
    ok(fdflag_close_exec(out1, true), "set fd 8 to close-on-exec");
    ok(fdflag_close_exec(out2, true), "set fd 9 to close-on-exec");
    ok(fdflag_close_exec(out2, false), "set fd 9 back to regular");

    /*
     * Fork, child closes the open socket and then tries to connect, parent
     * calls listen() and accept() on it.  Parent will then set the socket
     * non-blocking and try to read from it to see what happens, then write to
     * the socket and close it, triggering the child close and exit.
     *
     * Before the child exits, it will exec a shell that will print "no" to
     * the duplicate of stdout that the parent created and then the ok to
     * regular stdout.
     */
    child = fork();
    if (child < 0) {
        sysbail("fork failed");
    } else if (child != 0) {
        size = sizeof(sin);
        data = accept(master, (struct sockaddr *) &sin, &size);
        close(master);
        if (data < 0)
            sysbail("accept failed");
        ok(fdflag_nonblocking(data, true), "set socket non-blocking");
        status = read(data, buffer, sizeof(buffer));
        is_int(-1, status, "got -1 from non-blocking read");
        is_int(EAGAIN, errno, "...with EAGAIN errno");
        if (write(data, buffer, sizeof(buffer)) < (ssize_t) sizeof(buffer))
            sysbail("write failed");
        close(data);
        testnum += 2;
    } else {
        data = socket(AF_INET, SOCK_STREAM, 0);
        if (data < 0)
            sysbail("child socket failed");
        if (connect(data, (struct sockaddr *) &sin, sizeof(sin)) < 0)
            sysbail("child connect failed");
        if (read(data, buffer, sizeof(buffer)) < (ssize_t) sizeof(buffer))
            sysbail("read failed");
        fclose(stderr);
        execlp("sh", "sh", "-c",
               "printf 'not ' >&8; echo ok 7; echo 'ok 8' >&9", (char *) 0);
        sysbail("exec failed");
    }
    waitpid(child, NULL, 0);
    exit(0);
}
