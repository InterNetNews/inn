/* $Id$ */
/* Test suite for error handling routines. */

#include "config.h"
#include "clibrary.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "inn/messages.h"
#include "libinn.h"

#define END     (char *) 0

/* Test function type. */
typedef void (*test_function_t)(void);

/* Fork and execute the provided function, connecting stdout and stderr to a
   pipe.  Captures the output into the provided buffer and returns the exit
   status as a waitpid status value. */
static int
run_test(test_function_t function, char *buf, size_t buflen)
{
    int fds[2];
    pid_t child;
    ssize_t count, status;

    /* Flush stdout before we start to avoid odd forking issues. */
    fflush(stdout);

    /* Set up the pipe and call the function, collecting its output. */
    if (pipe(fds) == -1)
        sysdie("can't create pipe");
    child = fork();
    if (child == (pid_t) -1) {
        sysdie("can't fork");
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
        /* In the parent; close the extra file descriptor, read the output
           if any, and then collect the exit status. */
        close(fds[1]);
        count = 0;
        do {
            status = read(fds[0], buf + count, buflen - count - 1);
            if (status > 0)
                count += status;
        } while (status > 0);
        buf[count < 0 ? 0 : count] = '\0';
        if (waitpid(child, &status, 0) == (pid_t) -1)
            sysdie("waitpid failed");
    }
    return status;
}

/* Test functions. */
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

static void log(int len, const char *format, va_list args, int error) {
    fprintf(stderr, "%d %d ", len, error);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
}

static void test12(void) {
    message_handlers_warn(1, log);
    warn("warning");
}
static void test13(void) {
    message_handlers_die(1, log);
    die("fatal");
}
static void test14(void) {
    message_handlers_warn(2, log, log);
    errno = EPERM;
    syswarn("warning");
}
static void test15(void) {
    message_handlers_die(2, log, log);
    message_fatal_cleanup = return10;
    errno = EPERM;
    sysdie("fatal");
}
static void test16(void) {
    message_handlers_warn(2, message_log_stderr, log);
    message_program_name = "test16";
    errno = EPERM;
    syswarn("warning");
}
static void test17(void) { notice("notice"); }
static void test18(void) {
    message_program_name = "test18";
    notice("notice");
}
static void test19(void) { trace(TRACE_PROGRAM, "tracing"); }
static void test20(void) { debug("debug"); }
static void test21(void) {
    message_handlers_notice(1, log);
    notice("foo");
}
static void test22(void) {
    message_handlers_trace(1, log);
    message_trace_enable(TRACE_PROGRAM, true);
    trace(TRACE_PROGRAM, "foo");
    trace(TRACE_NETWORK, "bar");
}
static void test23(void) {
    message_handlers_debug(1, message_log_stdout);
    message_program_name = "test23";
    debug("baz");
}

/* Given the test number, intended exit status and message, and the function
   to run, print ok or not ok. */
static void
test_error(int n, int status, const char *output, test_function_t function)
{
    int real_status;
    char buf[256];
    int succeeded = 1;

    real_status = run_test(function, buf, sizeof(buf));
    if (!WIFEXITED(real_status) || status != WEXITSTATUS(real_status)) {
        printf("  unexpected exit status %d\n", real_status);
        succeeded = 0;
    }
    if (strcmp(output, buf)) {
        printf("  unexpected output: %s", buf);
        printf("    expected output: %s", output);
        succeeded = 0;
    }
    printf("%sok %d\n", succeeded ? "" : "not ", n);
}

/* Given the test number, intended status, intended message sans the
   appended strerror output, errno, and the function to run, print ok or not
   ok. */
static void
test_strerror(int n, int status, const char *output, int error,
              test_function_t function)
{
    char *full_output;

    full_output = concat(output, ": ", strerror(error), "\n", END);
    test_error(n, status, full_output, function);
    free(full_output);
}

/* Run the tests. */
int
main(void)
{
    char buff[32];

    puts("23");

    test_error(1, 0, "warning\n", test1);
    test_error(2, 1, "fatal\n", test2);
    test_strerror(3, 0, "permissions", EPERM, test3);
    test_strerror(4, 1, "fatal access", EACCES, test4);
    test_error(5, 0, "test5: warning\n", test5);
    test_error(6, 1, "test6: fatal\n", test6);
    test_strerror(7, 0, "test7: perms 7", EPERM, test7);
    test_strerror(8, 1, "test8: fatal", EACCES, test8);
    test_error(9, 10, "fatal\n", test9);
    test_strerror(10, 10, "fatal perm", EPERM, test10);
    test_strerror(11, 10, "1st test11: fatal", EPERM, test11);
    test_error(12, 0, "7 0 warning\n", test12);
    test_error(13, 1, "5 0 fatal\n", test13);

    sprintf(buff, "%d", EPERM);

    test_error(14, 0,
               concat("7 ", buff, " warning\n7 ", buff, " warning\n", END),
               test14);
    test_error(15, 10,
               concat("5 ", buff, " fatal\n5 ", buff, " fatal\n", END),
               test15);
    test_error(16, 0,
               concat("test16: warning: ", strerror(EPERM), "\n7 ", buff,
                      " warning\n", END),
               test16);

    test_error(17, 0, "notice\n", test17);
    test_error(18, 0, "test18: notice\n", test18);
    test_error(19, 0, "", test19);
    test_error(20, 0, "", test20);
    test_error(21, 0, "3 0 foo\n", test21);
    test_error(22, 0, "3 0 foo\n", test22);
    test_error(23, 0, "test23: baz\n", test23);

    return 0;
}
