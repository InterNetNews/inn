/* $Id$ */
/* confparse test suite. */

#include "config.h"
#include "clibrary.h"
#include <unistd.h>

#include "inn/confparse.h"
#include "libinn.h"
#include "libtest.h"

/* Used to accumulate error messages from the parser. */
static char *errors = NULL;

/* Error handler that appends errors to the errors global. */
static void
string_error(int len, const char *format, va_list args, int error UNUSED)
{
    char *message;

    message = xmalloc(len + 1);
    vsnprintf(message, len + 1, format, args);
    if (errors == NULL) {
        errors = concat(message, "\n", (char *) 0);
    } else {
        char *new_errors;

        new_errors = concat(errors, message, "\n", (char *) 0);
        free(errors);
        errors = new_errors;
    }
    free(message);
}

/* Given a FILE *, read from that file, putting the results into a newly
   allocated buffer, until encountering a line consisting solely of "===".
   Returns the buffer, NULL on end of file, dies on error. */
static char *
read_section(FILE *file)
{
    char buf[1024] = "";
    char *data = NULL;
    char *status;

    status = fgets(buf, sizeof(buf), file);
    if (status == NULL)
        return false;
    while (1) {
        if (status == NULL)
            die("Unexpected end of file while reading tests");
        if (strcmp(buf, "===\n") == 0)
            break;
        if (data == NULL) {
            data = xstrdup(buf);
        } else {
            char *new_data;

            new_data = concat(data, buf, (char *) 0);
            free(data);
            data = new_data;
        }
        status = fgets(buf, sizeof(buf), file);
    }
    return data;
}

/* Read from the given file a configuration file and write it out to
   config/tmp.  Returns true on success, false on end of file, and dies on
   any error. */
static bool
write_test_config(FILE *file)
{
    FILE *tmp;
    char *config;

    config = read_section(file);
    if (config == NULL)
        return false;
    tmp = fopen("config/tmp", "w");
    if (tmp == NULL)
        sysdie("Cannot create config/tmp");
    if (fputs(config, tmp) == EOF)
        sysdie("Write error while writing to config/tmp");
    fclose(tmp);
    free(config);
    return true;
}

/* Read in a configuration file from the provided FILE *, write it to disk,
   parse the temporary config file, and return the resulting config_group in
   the pointer passed as the second parameter.  Returns true on success,
   false on end of file.  The group is parsed with a warning handler that
   saves any warning messages into the errors global. */
static bool
parse_test_config(FILE *file, config_group *group)
{
    if (!write_test_config(file))
        return false;

    if (errors != NULL) {
        free(errors);
        errors = NULL;
    }
    warn_set_handlers(1, string_error);

    *group = config_parse_file("config/tmp");
    unlink("config/tmp");
    return true;
}

/* Test the error test cases in config/errors, ensuring that they all fail
   to parse and match the expected error messages.  Takes the current test
   count and returns the new test count. */
static int
test_errors(int n)
{
    FILE *errfile;
    char *expected;
    config_group group;

    errfile = fopen("config/errors", "r");
    if (errfile == NULL)
        sysdie("Cannot open config/errors");
    while (parse_test_config(errfile, &group)) {
        expected = read_section(errfile);
        if (expected == NULL)
            die("Unexpected end of file while reading error tests");
        ok(n++, group == NULL);
        ok_string(n++, expected, errors);
        free(expected);
    }
    fclose(errfile);
    return n;
}

/* Test the warning test cases in config/warningss, ensuring that they all
   parse successfully and match the expected error messages.  Takes the
   current test count and returns the new test count. */
static int
test_warnings(int n)
{
    FILE *warnfile;
    char *expected;
    config_group group;

    warnfile = fopen("config/warnings", "r");
    if (warnfile == NULL)
        sysdie("Cannot open config/warnings");
    while (parse_test_config(warnfile, &group)) {
        expected = read_section(warnfile);
        if (expected == NULL)
            die("Unexpected end of file while reading error tests");
        ok(n++, group != NULL);
        ok_string(n++, expected, errors);
        free(expected);
    }
    fclose(warnfile);
    return n;
}

int
main(void)
{
    config_group group;
    bool b_value = false;
    long l_value = 1;
    const char *s_value;
    int n;

    puts("61");

    if (access("config/valid", F_OK) < 0)
        if (access("lib/config/valid", F_OK) == 0)
            chdir("lib");
    group = config_parse_file("config/valid");
    ok(1, group != NULL);
    if (group == NULL)
        exit(1);

    /* Booleans. */
    ok(2, config_param_boolean(group, "param1", &b_value));
    ok(3, b_value);
    b_value = false;
    ok(4, config_param_boolean(group, "param2", &b_value));
    ok(5, b_value);
    b_value = false;
    ok(6, config_param_boolean(group, "param3", &b_value));
    ok(7, b_value);
    ok(8, config_param_boolean(group, "param4", &b_value));
    ok(9, !b_value);
    b_value = true;
    ok(10, config_param_boolean(group, "param5", &b_value));
    ok(11, !b_value);
    b_value = true;
    ok(12, config_param_boolean(group, "param6", &b_value));
    ok(13, !b_value);

    /* Integers. */
    ok(14, config_param_integer(group, "int1", &l_value));
    ok(15, l_value == 0);
    ok(16, config_param_integer(group, "int2", &l_value));
    ok(17, l_value == -3);
    ok(18, !config_param_integer(group, "int3", &l_value));
    ok(19, l_value == -3);
    ok(20, config_param_integer(group, "int4", &l_value));
    ok(21, l_value == 5000);
    ok(22, config_param_integer(group, "int5", &l_value));
    ok(23, l_value == 2147483647L);
    ok(24, config_param_integer(group, "int6", &l_value));
    ok(25, l_value == (-2147483647L - 1));

    /* Strings. */
    ok(26, config_param_string(group, "string1", &s_value));
    ok_string(27, s_value, "foo");
    ok(28, config_param_string(group, "string2", &s_value));
    ok_string(29, s_value, "bar");
    ok(30, config_param_string(group, "string3", &s_value));
    ok_string(31, s_value, "this is a test");
    ok(32, config_param_string(group, "string4", &s_value));
    ok_string(33, s_value, "this is a test");
    ok(34, config_param_string(group, "string5", &s_value));
    ok_string(35, s_value,
              "this is \a\b\f\n\r\t\v a test \' of \" escapes \?\\");
    ok(36, config_param_string(group, "string6", &s_value));
    ok_string(37, s_value, "# this is not a comment");
    ok(38, config_param_string(group, "string7", &s_value));
    ok_string(39, s_value, "lost \nyet?");

    config_free(group);

    /* Errors. */
    n = test_errors(40);
    n = test_warnings(n);

    return 0;
}
