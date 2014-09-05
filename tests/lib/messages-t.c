/*
 * Test suite for error handling routines.
 *
 * $Id$
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <rra@stanford.edu>
 * Copyright 2002, 2004, 2005 Russ Allbery <rra@stanford.edu>
 * Copyright 2009, 2010
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
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "inn/messages.h"
#include "inn/libinn.h"
#include "tap/basic.h"

typedef void (*test_function_type)(void);
void is_function_output(test_function_type, int status, const char *output,
                        const char *format, ...)
    __attribute__((__format__(printf, 4, 5)));

/*
 * Given a function, an expected exit status, and expected output, runs that
 * function in a subprocess, capturing stdout and stderr via a pipe, and
 * compare the combination of stdout and stderr with the expected output and
 * the exit status with the expected status.  Expects the function to always
 * exit (not die from a signal).
 */
void
is_function_output(test_function_type function, int status, const char *output,
                   const char *format, ...)
{
    int fds[2];
    pid_t child;
    char *buf, *msg;
    ssize_t count, ret, buflen;
    int rval;
    va_list args;

    /* Flush stdout before we start to avoid odd forking issues. */
    fflush(stdout);

    /* Set up the pipe and call the function, collecting its output. */
    if (pipe(fds) == -1)
        sysbail("can't create pipe");
    child = fork();
    if (child == (pid_t) -1) {
        sysbail("can't fork");
    } else if (child == 0) {
        /* In child.  Set up our stdout and stderr. */
        close(fds[0]);
        if (dup2(fds[1], 1) == -1)
            _exit(255);
        if (dup2(fds[1], 2) == -1)
            _exit(255);

        /* Now, run the function and exit successfully if it returns. */
        (*function)();
        fflush(stdout);
        _exit(0);
    } else {
        /*
         * In the parent; close the extra file descriptor, read the output if
         * any, and then collect the exit status.
         */
        close(fds[1]);
        buflen = BUFSIZ;
        buf = xmalloc(buflen);
        count = 0;
        do {
            ret = read(fds[0], buf + count, buflen - count - 1);
            if (ret > 0)
                count += ret;
            if (count >= buflen - 1) {
                buflen += BUFSIZ;
                buf = xrealloc(buf, buflen);
            }
        } while (ret > 0);
        buf[count < 0 ? 0 : count] = '\0';
        if (waitpid(child, &rval, 0) == (pid_t) -1)
            sysbail("waitpid failed");
    }

    /* Now, check the results against what we expected. */
    va_start(args, format);
    xvasprintf(&msg, format, args);
    va_end(args);
    ok(WIFEXITED(rval), "%s (exited)", msg);
    is_int(status, WEXITSTATUS(rval), "%s (status)", msg);
    is_string(output, buf, "%s (output)", msg);
    free(buf);
    free(msg);
}

/*
 * Test functions.
 */
static void test1(void) { warn("warning"); }
static void test2(void) { die("fatal"); }
static void test3(void) { errno = EPERM; syswarn("permissions"); }
static void test4(void) { errno = EACCES; sysdie("fatal access"); }
static void test5(void) {
    message_program_name = "test5";
    warn("warning");
}
static void test6(void) {
    message_program_name = "test6";
    die("fatal");
}
static void test7(void) {
    message_program_name = "test7";
    errno = EPERM;
    syswarn("perms %d", 7);
}
static void test8(void) {
    message_program_name = "test8";
    errno = EACCES;
    sysdie("%st%s", "fa", "al");
}

static int return10(void) { return 10; }

static void test9(void) {
    message_fatal_cleanup = return10;
    die("fatal");
}
static void test10(void) {
    message_program_name = 0;
    message_fatal_cleanup = return10;
    errno = EPERM;
    sysdie("fatal perm");
}
static void test11(void) {
    message_program_name = "test11";
    message_fatal_cleanup = return10;
    errno = EPERM;
    fputs("1st ", stdout);
    sysdie("fatal");
}

static void log_msg(int len, const char *format, va_list args, int error) {
    fprintf(stderr, "%d %d ", len, error);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
}

static void test12(void) {
    message_handlers_warn(1, log_msg);
    warn("warning");
}
static void test13(void) {
    message_handlers_die(1, log_msg);
    die("fatal");
}
static void test14(void) {
    message_handlers_warn(2, log_msg, log_msg);
    errno = EPERM;
    syswarn("warning");
}
static void test15(void) {
    message_handlers_die(2, log_msg, log_msg);
    message_fatal_cleanup = return10;
    errno = EPERM;
    sysdie("fatal");
}
static void test16(void) {
    message_handlers_warn(2, message_log_stderr, log_msg);
    message_program_name = "test16";
    errno = EPERM;
    syswarn("warning");
}
static void test17(void) { notice("notice"); }
static void test18(void) {
    message_program_name = "test18";
    notice("notice");
}
static void test19(void) { debug("debug"); }
static void test20(void) {
    message_handlers_notice(1, log_msg);
    notice("foo");
}
static void test21(void) {
    message_handlers_debug(1, message_log_stdout);
    message_program_name = "test23";
    debug("baz");
}
static void test22(void) {
    message_handlers_die(0);
    die("hi mom!");
}
static void test23(void) {
    message_handlers_warn(0);
    warn("this is a test");
}
static void test24(void) {
    notice("first");
    message_handlers_notice(0);
    notice("second");
    message_handlers_notice(1, message_log_stdout);
    notice("third");
}


/*
 * Given the intended status, intended message sans the appended strerror
 * output, errno, and the function to run, check the output.
 */
static void
test_strerror(int status, const char *output, int error,
              test_function_type function)
{
    char *full_output, *name;

    full_output = concat(output, ": ", strerror(error), "\n", (char *) NULL);
    xasprintf(&name, "strerror %lu", testnum / 3 + 1);
    is_function_output(function, status, full_output, "%s", name);
    free(full_output);
    free(name);
}


/*
 * Run the tests.
 */
int
main(void)
{
    char buff[32];
    char *output;

    plan(24 * 3);

    is_function_output(test1, 0, "warning\n", "test1");
    is_function_output(test2, 1, "fatal\n", "test2");
    test_strerror(0, "permissions", EPERM, test3);
    test_strerror(1, "fatal access", EACCES, test4);
    is_function_output(test5, 0, "test5: warning\n", "test5");
    is_function_output(test6, 1, "test6: fatal\n", "test6");
    test_strerror(0, "test7: perms 7", EPERM, test7);
    test_strerror(1, "test8: fatal", EACCES, test8);
    is_function_output(test9, 10, "fatal\n", "test9");
    test_strerror(10, "fatal perm", EPERM, test10);
    test_strerror(10, "1st test11: fatal", EPERM, test11);
    is_function_output(test12, 0, "7 0 warning\n", "test12");
    is_function_output(test13, 1, "5 0 fatal\n", "test13");

    sprintf(buff, "%d", EPERM);

    xasprintf(&output, "7 %d warning\n7 %d warning\n", EPERM, EPERM);
    is_function_output(test14, 0, output, "test14");
    free(output);
    xasprintf(&output, "5 %d fatal\n5 %d fatal\n", EPERM, EPERM);
    is_function_output(test15, 10, output, "test15");
    free(output);
    xasprintf(&output, "test16: warning: %s\n7 %d warning\n", strerror(EPERM),
              EPERM);
    is_function_output(test16, 0, output, "test16");
    free(output);

    is_function_output(test17, 0, "notice\n", "test17");
    is_function_output(test18, 0, "test18: notice\n", "test18");
    is_function_output(test19, 0, "", "test19");
    is_function_output(test20, 0, "3 0 foo\n", "test20");
    is_function_output(test21, 0, "test23: baz\n", "test21");

    /* Make sure that it's possible to turn off a message type entirely. */ 
    is_function_output(test22, 1, "", "test22");
    is_function_output(test23, 0, "", "test23");
    is_function_output(test24, 0, "first\nthird\n", "test24");

    return 0;
}
