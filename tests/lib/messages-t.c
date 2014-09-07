/* $Id$
 *
 * Test suite for error handling routines.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2002, 2004, 2005 Russ Allbery <eagle@eyrie.org>
 * Copyright 2009, 2010, 2011, 2012
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
#include "portable/wait.h"

#include "tap/basic.h"
#include "tap/process.h"
#include "inn/macros.h"
#include "inn/messages.h"
#include "inn/xmalloc.h"


/*
 * Test functions.
 */
static void test1(void *data UNUSED) { warn("warning"); }
static void test2(void *data UNUSED) { die("fatal"); }
static void test3(void *data UNUSED) { errno = EPERM; syswarn("permissions"); }
static void test4(void *data UNUSED) {
    errno = EACCES;
    sysdie("fatal access");
}
static void test5(void *data UNUSED) {
    message_program_name = "test5";
    warn("warning");
}
static void test6(void *data UNUSED) {
    message_program_name = "test6";
    die("fatal");
}
static void test7(void *data UNUSED) {
    message_program_name = "test7";
    errno = EPERM;
    syswarn("perms %d", 7);
}
static void test8(void *data UNUSED) {
    message_program_name = "test8";
    errno = EACCES;
    sysdie("%st%s", "fa", "al");
}

static int return10(void) { return 10; }

static void test9(void *data UNUSED) {
    message_fatal_cleanup = return10;
    die("fatal");
}
static void test10(void *data UNUSED) {
    message_program_name = 0;
    message_fatal_cleanup = return10;
    errno = EPERM;
    sysdie("fatal perm");
}
static void test11(void *data UNUSED) {
    message_program_name = "test11";
    message_fatal_cleanup = return10;
    errno = EPERM;
    fputs("1st ", stdout);
    sysdie("fatal");
}

static void log_msg(size_t len, const char *format, va_list args, int error) {
    fprintf(stderr, "%lu %d ", (unsigned long) len, error);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
}

static void test12(void *data UNUSED) {
    message_handlers_warn(1, log_msg);
    warn("warning");
}
static void test13(void *data UNUSED) {
    message_handlers_die(1, log_msg);
    die("fatal");
}
static void test14(void *data UNUSED) {
    message_handlers_warn(2, log_msg, log_msg);
    errno = EPERM;
    syswarn("warning");
}
static void test15(void *data UNUSED) {
    message_handlers_die(2, log_msg, log_msg);
    message_fatal_cleanup = return10;
    errno = EPERM;
    sysdie("fatal");
}
static void test16(void *data UNUSED) {
    message_handlers_warn(2, message_log_stderr, log_msg);
    message_program_name = "test16";
    errno = EPERM;
    syswarn("warning");
}
static void test17(void *data UNUSED) { notice("notice"); }
static void test18(void *data UNUSED) {
    message_program_name = "test18";
    notice("notice");
}
static void test19(void *data UNUSED) { debug("debug"); }
static void test20(void *data UNUSED) {
    message_handlers_notice(1, log_msg);
    notice("foo");
}
static void test21(void *data UNUSED) {
    message_handlers_debug(1, message_log_stdout);
    message_program_name = "test23";
    debug("baz");
}
static void test22(void *data UNUSED) {
    message_handlers_die(0);
    die("hi mom!");
}
static void test23(void *data UNUSED) {
    message_handlers_warn(0);
    warn("this is a test");
}
static void test24(void *data UNUSED) {
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

    xasprintf(&full_output, "%s: %s\n", output, strerror(error));
    xasprintf(&name, "strerror %lu", testnum / 3 + 1);
    is_function_output(function, NULL, status, full_output, "%s", name);
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

    is_function_output(test1, NULL, 0, "warning\n", "test1");
    is_function_output(test2, NULL, 1, "fatal\n", "test2");
    test_strerror(0, "permissions", EPERM, test3);
    test_strerror(1, "fatal access", EACCES, test4);
    is_function_output(test5, NULL, 0, "test5: warning\n", "test5");
    is_function_output(test6, NULL, 1, "test6: fatal\n", "test6");
    test_strerror(0, "test7: perms 7", EPERM, test7);
    test_strerror(1, "test8: fatal", EACCES, test8);
    is_function_output(test9, NULL, 10, "fatal\n", "test9");
    test_strerror(10, "fatal perm", EPERM, test10);
    test_strerror(10, "1st test11: fatal", EPERM, test11);
    is_function_output(test12, NULL, 0, "7 0 warning\n", "test12");
    is_function_output(test13, NULL, 1, "5 0 fatal\n", "test13");

    sprintf(buff, "%d", EPERM);

    xasprintf(&output, "7 %d warning\n7 %d warning\n", EPERM, EPERM);
    is_function_output(test14, NULL, 0, output, "test14");
    free(output);
    xasprintf(&output, "5 %d fatal\n5 %d fatal\n", EPERM, EPERM);
    is_function_output(test15, NULL, 10, output, "test15");
    free(output);
    xasprintf(&output, "test16: warning: %s\n7 %d warning\n", strerror(EPERM),
              EPERM);
    is_function_output(test16, NULL, 0, output, "test16");
    free(output);

    is_function_output(test17, NULL, 0, "notice\n", "test17");
    is_function_output(test18, NULL, 0, "test18: notice\n", "test18");
    is_function_output(test19, NULL, 0, "", "test19");
    is_function_output(test20, NULL, 0, "3 0 foo\n", "test20");
    is_function_output(test21, NULL, 0, "test23: baz\n", "test21");

    /* Make sure that it's possible to turn off a message type entirely. */ 
    is_function_output(test22, NULL, 1, "", "test22");
    is_function_output(test23, NULL, 0, "", "test23");
    is_function_output(test24, NULL, 0, "first\nthird\n", "test24");

    return 0;
}
