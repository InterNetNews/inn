/* $Id$ */
/* Test suite for error handling routines. */

#include "config.h"
#include "clibrary.h"
#include <errno.h>
#if HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

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
    int fds[2], status;
    pid_t child;
    ssize_t count;

    /* Flush stdout before we start to avoid odd forking issues. */
    fflush(stdout);

    /* Set up the pipe and call the function, collecting its output. */
    if (pipe(fds) == -1) sysdie("can't create pipe");
    child = fork();
    if (child == (pid_t) -1) {
        sysdie("can't fork");
    } else if (child == 0) {
        /* In child.  Set up our stdout and stderr. */
        close(fds[0]);
        if (dup2(fds[1], 1) == -1) _exit(255);
        if (dup2(fds[1], 2) == -1) _exit(255);

        /* Now, run the function and exit successfully if it returns. */
        (*function)();
        _exit(0);
    } else {
        /* In the parent; close the extra file descriptor, read the output
           if any, and then collect the exit status. */
        close(fds[1]);
        count = read(fds[0], buf, buflen - 1);
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
    error_program_name = "test5";
    warn("warning");
}
static void test6(void) {
    error_program_name = "test6";
    die("fatal");
}
static void test7(void) {
    error_program_name = "test7";
    errno = EPERM;
    syswarn("perms %d", 7);
}
static void test8(void) {
    error_program_name = "test8";
    errno = EACCES;
    sysdie("%st%s", "fa", "al");
}

static int return10(void) { return 10; }

static void test9(void) {
    error_fatal_cleanup = return10;
    die("fatal");
}
static void test10(void) {
    error_program_name = 0;
    error_fatal_cleanup = return10;
    errno = EPERM;
    sysdie("fatal perm");
}
static void test11(void) {
    error_program_name = "test11";
    error_fatal_cleanup = return10;
    errno = EPERM;
    fputs("1st ", stdout);
    sysdie("fatal");
}

static int log(int len, const char *format, va_list args, int error) {
    fprintf(stderr, "%d %d ", len, error);
    len = vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    return len;
}

static void test12(void) {
    warn_set_handlers(1, log);
    warn("warning");
}
static void test13(void) {
    die_set_handlers(1, log);
    die("fatal");
}
static void test14(void) {
    warn_set_handlers(2, log, log);
    errno = EPERM;
    syswarn("warning");
}
static void test15(void) {
    die_set_handlers(2, log, log);
    error_fatal_cleanup = return10;
    errno = EPERM;
    sysdie("fatal");
}

/* Given the test number, intended exit status and message, and the actual
   exit status and message, print ok or not ok. */
static void
ok(int n, int status, const char *output, test_function_t function)
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

/* Run the tests. */
int
main(void)
{
    char buff[32];

    puts("15");

    ok(1, 0, "warning\n", test1);
    ok(2, 1, "fatal\n", test2);
    ok(3, 0, concat("permissions: ", strerror(EPERM), "\n", END), test3);
    ok(4, 1, concat("fatal access: ", strerror(EACCES), "\n", END), test4);
    ok(5, 0, "test5: warning\n", test5);
    ok(6, 1, "test6: fatal\n", test6);
    ok(7, 0, concat("test7: perms 7: ", strerror(EPERM), "\n", END), test7);
    ok(8, 1, concat("test8: fatal: ", strerror(EACCES), "\n", END), test8);
    ok(9, 10, "fatal\n", test9);
    ok(10, 10, concat("fatal perm: ", strerror(EPERM), "\n", END), test10);
    ok(11, 10, concat("1st test11: fatal: ", strerror(EPERM), "\n", END),
       test11);
    ok(12, 0, "0 0 warning\n", test12);
    ok(13, 1, "0 0 fatal\n", test13);

    sprintf(buff, "%d", EPERM);

    ok(14, 0, concat("0 ", buff, " warning\n7 ", buff, " warning\n", END),
       test14);
    ok(15, 10, concat("0 ", buff, " fatal\n5 ", buff, " fatal\n", END),
       test15);

    return 0;
}
