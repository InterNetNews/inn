/* $Id$ */
/* confparse test suite. */

#include "config.h"
#include "clibrary.h"

#include "inn/confparse.h"
#include "inn/messages.h"
#include "inn/vector.h"
#include "inn/libinn.h"
#include "tap/basic.h"
#include "tap/float.h"

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

/* Parse a given config file with errors, setting the appropriate error
   handler for the duration of the parse to save errors into the errors
   global.  Returns the resulting config_group. */
static struct config_group *
parse_error_config(const char *filename)
{
    struct config_group *group;

    errors_capture();
    group = config_parse_file(filename);
    errors_uncapture();
    return group;
}

/* Read in a configuration file from the provided FILE *, write it to disk,
   parse the temporary config file, and return the resulting config_group in
   the pointer passed as the second parameter.  Returns true on success,
   false on end of file. */
static bool
parse_test_config(FILE *file, struct config_group **group)
{
    if (!write_test_config(file))
        return false;
    *group = parse_error_config("config/tmp");
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
    struct config_group *group;

    if (symlink(".", "config/link") < 0)
        sysdie("Cannot create config/link symlink");
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
        if (group != NULL)
            config_free(group);
    }
    fclose(errfile);
    unlink("config/link");
    return n;
}

/* Test the warning test cases in config/warnings, ensuring that they all
   parse successfully and match the expected error messages.  Takes the
   current test count and returns the new test count. */
static int
test_warnings(int n)
{
    FILE *warnfile;
    char *expected;
    struct config_group *group;

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
        if (group != NULL)
            config_free(group);
    }
    fclose(warnfile);
    return n;
}

/* Test the warning test cases in config/warn-bool, ensuring that they all
   parse successfully and produce the expected error messages when retrieved
   as bools.  Takes the current test count and returns the new test count. */
static int
test_warnings_bool(int n)
{
    FILE *warnfile;
    char *expected;
    struct config_group *group;
    bool b_value = false;

    warnfile = fopen("config/warn-bool", "r");
    if (warnfile == NULL)
        sysdie("Cannot open config/warn-bool");
    while (parse_test_config(warnfile, &group)) {
        expected = read_section(warnfile);
        if (expected == NULL)
            die("Unexpected end of file while reading error tests");
        ok(n++, group != NULL);
        ok(n++, errors == NULL);
        errors_capture();
        ok(n++, !config_param_boolean(group, "parameter", &b_value));
        ok_string(n++, expected, errors);
        errors_uncapture();
        free(expected);
        if (group != NULL)
            config_free(group);
    }
    fclose(warnfile);
    return n;
}

/* Test the warning test cases in config/warn-int, ensuring that they all
   parse successfully and produce the expected error messages when retrieved
   as signed numbers.  Takes the current test count and returns the new test
   count. */
static int
test_warnings_int(int n)
{
    FILE *warnfile;
    char *expected;
    struct config_group *group;
    long l_value = 1;

    warnfile = fopen("config/warn-int", "r");
    if (warnfile == NULL)
        sysdie("Cannot open config/warn-int");
    while (parse_test_config(warnfile, &group)) {
        expected = read_section(warnfile);
        if (expected == NULL)
            die("Unexpected end of file while reading error tests");
        ok(n++, group != NULL);
        ok(n++, errors == NULL);
        errors_capture();
        ok(n++, !config_param_signed_number(group, "parameter", &l_value));
        ok_string(n++, expected, errors);
        errors_uncapture();
        free(expected);
        if (group != NULL)
            config_free(group);
    }
    fclose(warnfile);
    return n;
}

/* Test the warning test cases in config/warn-uint, ensuring that they all
   parse successfully and produce the expected error messages when retrieved
   as usigned numbers.  Takes the current test count and returns the new test
   count. */
static int
test_warnings_uint(int n)
{
    FILE *warnfile;
    char *expected;
    struct config_group *group;
    unsigned long lu_value = 1;

    warnfile = fopen("config/warn-uint", "r");
    if (warnfile == NULL)
        sysdie("Cannot open config/warn-uint");
    while (parse_test_config(warnfile, &group)) {
        expected = read_section(warnfile);
        if (expected == NULL)
            die("Unexpected end of file while reading error tests");
        ok(n++, group != NULL);
        ok(n++, errors == NULL);
        errors_capture();
        ok(n++, !config_param_unsigned_number(group, "parameter", &lu_value));
        ok_string(n++, expected, errors);
        errors_uncapture();
        free(expected);
        if (group != NULL)
            config_free(group);
    }
    fclose(warnfile);
    return n;
}

/* Test the warning test cases in config/warn-real, ensuring that they all
   parse successfully and produce the expected error messages when retrieved
   as reals.  Takes the current test count and returns the new test count. */
static int
test_warnings_real(int n)
{
    FILE *warnfile;
    char *expected;
    struct config_group *group;
    double d_value;

    warnfile = fopen("config/warn-real", "r");
    if (warnfile == NULL)
        sysdie("Cannot open config/warn-real");
    while (parse_test_config(warnfile, &group)) {
        expected = read_section(warnfile);
        if (expected == NULL)
            die("Unexpected end of file while reading error tests");
        ok(n++, group != NULL);
        ok(n++, errors == NULL);
        errors_capture();
        ok(n++, !config_param_real(group, "parameter", &d_value));
        ok_string(n++, expected, errors);
        errors_uncapture();
        free(expected);
        if (group != NULL)
            config_free(group);
    }
    fclose(warnfile);
    return n;
}

/* Test the warning test cases in config/warn-string, ensuring that they all
   parse successfully and produce the expected error messages when retrieved
   as strings.  Takes the current test count and returns the new test count. */
static int
test_warnings_string(int n)
{
    FILE *warnfile;
    char *expected;
    struct config_group *group;
    const char *s_value = NULL;

    warnfile = fopen("config/warn-string", "r");
    if (warnfile == NULL)
        sysdie("Cannot open config/warn-string");
    while (parse_test_config(warnfile, &group)) {
        expected = read_section(warnfile);
        if (expected == NULL)
            die("Unexpected end of file while reading error tests");
        ok(n++, group != NULL);
        ok(n++, errors == NULL);
        errors_capture();
        ok(n++, !config_param_string(group, "parameter", &s_value));
        ok_string(n++, expected, errors);
        errors_uncapture();
        free(expected);
        if (group != NULL)
            config_free(group);
    }
    fclose(warnfile);
    return n;
}

/* Test the warning test cases in config/warn-list, ensuring that they all
   parse successfully and produce the expected error messages when retrieved
   as lists.  Takes the current test count and returns the new test count. */
static int
test_warnings_list(int n)
{
    FILE *warnfile;
    char *expected;
    struct config_group *group;
    const struct vector *v_value = NULL;

    warnfile = fopen("config/warn-list", "r");
    if (warnfile == NULL)
        sysdie("Cannot open config/warn-list");
    while (parse_test_config(warnfile, &group)) {
        expected = read_section(warnfile);
        if (expected == NULL)
            die("Unexpected end of file while reading error tests");
        ok(n++, group != NULL);
        ok(n++, errors == NULL);
        errors_capture();
        ok(n++, !config_param_list(group, "parameter", &v_value));
        ok_string(n++, expected, errors);
        errors_uncapture();
        free(expected);
        if (group != NULL)
            config_free(group);
    }
    fclose(warnfile);
    return n;
}

int
main(void)
{
    struct config_group *group, *subgroup;
    bool b_value = false;
    long l_value = 1;
    double d_value = 1;
    const char *s_value;
    const struct vector *v_value;
    struct vector *vector;
    char *long_param, *long_value;
    size_t length;
    int n;
    FILE *tmpconfig;

    test_init(373);

    if (access("../data/config/valid", F_OK) == 0)
        chdir("../data");
    else if (access("data/config/valid", F_OK) == 0)
        chdir("data");
    else if (access("tests/data/config/valid", F_OK) == 0)
        chdir("tests/data");
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
    ok(14, config_param_signed_number(group, "int1", &l_value));
    ok(15, l_value == 0);
    ok(16, config_param_signed_number(group, "int2", &l_value));
    ok(17, l_value == -3);
    ok(18, !config_param_signed_number(group, "int3", &l_value));
    ok(19, l_value == -3);
    ok(20, config_param_signed_number(group, "int4", &l_value));
    ok(21, l_value == 5000);
    ok(22, config_param_signed_number(group, "int5", &l_value));
    ok(23, l_value == 2147483647L);
    ok(24, config_param_signed_number(group, "int6", &l_value));
    ok(25, l_value == (-2147483647L - 1));

    /* Strings. */
    ok(26, config_param_string(group, "string1", &s_value));
    ok_string(27, "foo", s_value);
    ok(28, config_param_string(group, "string2", &s_value));
    ok_string(29, "bar", s_value);
    ok(30, config_param_string(group, "string3", &s_value));
    ok_string(31, "this is a test", s_value);
    ok(32, config_param_string(group, "string4", &s_value));
    ok_string(33, "this is a test", s_value);
    ok(34, config_param_string(group, "string5", &s_value));
    ok_string(35, "this is \a\b\f\n\r\t\v a test \' of \" escapes \?\\",
              s_value);
    ok(36, config_param_string(group, "string6", &s_value));
    ok_string(37, "# this is not a comment", s_value);
    ok(38, config_param_string(group, "string7", &s_value));
    ok_string(39, "lost \nyet?", s_value);

    config_free(group);

    /* Missing newline. */
    group = config_parse_file("config/no-newline");
    ok(40, group != NULL);
    if (group == NULL)
        ok_block(41, 2, false);
    else {
        ok(41, config_param_string(group, "parameter", &s_value));
        ok_string(42, "value", s_value);
        config_free(group);
    }

    /* Extremely long parameter and value. */
    tmpconfig = fopen("config/tmp", "w");
    if (tmpconfig == NULL)
        sysdie("cannot create config/tmp");
    long_param = xcalloc(20001, 1);
    memset(long_param, 'a', 20000);
    long_value = xcalloc(64 * 1024 + 1, 1);
    memset(long_value, 'b', 64 * 1024);
    fprintf(tmpconfig, "%s: \"%s\"; two: %s", long_param, long_value,
            long_value);
    fclose(tmpconfig);
    group = config_parse_file("config/tmp");
    ok(43, group != NULL);
    if (group == NULL)
        ok_block(44, 4, false);
    else {
        ok(44, config_param_string(group, long_param, &s_value));
        ok_string(45, long_value, s_value);
        ok(46, config_param_string(group, "two", &s_value));
        ok_string(47, long_value, s_value);
        config_free(group);
    }
    unlink("config/tmp");
    free(long_param);
    free(long_value);

    /* Parsing problems exactly on the boundary of a buffer.  This test caught
       a bug in the parser that caused it to miss the colon at the end of a
       parameter because the colon was the first character read in a new read
       of the file buffer. */
    tmpconfig = fopen("config/tmp", "w");
    if (tmpconfig == NULL)
        sysdie("cannot create config/tmp");
    length = 16 * 1024 - strlen(": baz\nfoo:");
    long_param = xcalloc(length + 1, 1);
    memset(long_param, 'c', length);
    fprintf(tmpconfig, "%s: baz\nfoo: bar\n", long_param);
    fclose(tmpconfig);
    group = config_parse_file("config/tmp");
    ok(48, group != NULL);
    if (group == NULL)
        ok_block(49, 4, false);
    else {
        ok(49, config_param_string(group, long_param, &s_value));
        ok_string(50, "baz", s_value);
        ok(51, config_param_string(group, "foo", &s_value));
        ok_string(52, "bar", s_value);
        config_free(group);
    }
    unlink("config/tmp");
    free(long_param);

    /* Alternate line endings. */
    group = config_parse_file("config/line-endings");
    ok(53, group != NULL);
    if (group == NULL)
        exit(1);
    ok(54, config_param_boolean(group, "param1", &b_value));
    ok(55, b_value);
    b_value = false;
    ok(56, config_param_boolean(group, "param2", &b_value));
    ok(57, b_value);
    b_value = false;
    ok(58, config_param_boolean(group, "param3", &b_value));
    ok(59, b_value);
    ok(60, config_param_boolean(group, "param4", &b_value));
    ok(61, !b_value);
    ok(62, config_param_signed_number(group, "int1", &l_value));
    ok(63, l_value == 0);
    ok(64, config_param_signed_number(group, "int2", &l_value));
    ok(65, l_value == -3);
    config_free(group);

    /* Listing parameters. */
    group = config_parse_file("config/simple");
    ok(66, group != NULL);
    if (group == NULL)
        exit(1);
    vector = config_params(group);
    ok_int(67, 2, vector->count);
    ok_int(68, 3, vector->allocated);
    if (strcmp(vector->strings[0], "foo") == 0)
        ok_string(69, "bar", vector->strings[1]);
    else if (strcmp(vector->strings[0], "bar") == 0)
        ok_string(69, "foo", vector->strings[1]);
    else
        ok(69, false);
    vector_free(vector);
    config_free(group);

    /* Lists. */
    group = config_parse_file("config/lists");
    ok(70, group != NULL);
    if (group == NULL)
        exit(1);
    ok(71, config_param_list(group, "vector1", &v_value));
    ok_int(72, 1, v_value->count);
    ok_string(73, "simple", v_value->strings[0]);
    ok(74, config_param_list(group, "vector2", &v_value));
    ok_int(75, 3, v_value->count);
    ok_string(76, "foo\tbar", v_value->strings[0]);
    ok_string(77, "baz", v_value->strings[1]);
    ok_string(78, "# this is not a comment", v_value->strings[2]);
    ok(79, config_param_list(group, "vector3", &v_value));
    ok_int(80, 0, v_value->count);
    ok(81, config_param_list(group, "vector4", &v_value));
    ok_int(82, 0, v_value->count);
    ok(83, config_param_list(group, "vector5", &v_value));
    ok_int(84, 1, v_value->count);
    ok_string(85, "baz", v_value->strings[0]);
    ok(86, config_param_list(group, "vector6", &v_value));
    ok_int(87, 1, v_value->count);
    ok_string(88, "bar baz", v_value->strings[0]);
    config_free(group);

    /* Groups. */
    group = config_parse_file("config/groups");
    ok(89, group != NULL);
    if (group == NULL)
        exit(1);
    subgroup = config_find_group(group, "test");
    ok(90, subgroup != NULL);
    ok_string(91, "test", config_group_type(subgroup));
    ok(92, config_param_boolean(subgroup, "value", &b_value));
    ok(93, b_value);
    subgroup = config_next_group(subgroup);
    ok(94, subgroup != NULL);
    ok_string(95, "test", config_group_type(subgroup));
    ok(96, config_group_tag(subgroup) == NULL);
    ok(97, config_param_boolean(subgroup, "value", &b_value));
    subgroup = config_next_group(subgroup);
    ok(98, subgroup != NULL);
    ok(99, config_param_signed_number(subgroup, "value", &l_value));
    ok_int(100, 2, l_value);
    subgroup = config_next_group(subgroup);
    ok(101, subgroup != NULL);
    ok(102, config_param_signed_number(subgroup, "value", &l_value));
    ok_int(103, 3, l_value);
    subgroup = config_find_group(subgroup, "test");
    ok(104, subgroup != NULL);
    ok(105, config_param_signed_number(subgroup, "value", &l_value));
    ok_int(106, 2, l_value);
    subgroup = config_next_group(subgroup);
    ok(107, subgroup != NULL);
    ok_string(108, "test", config_group_type(subgroup));
    ok_string(109, "final", config_group_tag(subgroup));
    ok(110, config_param_signed_number(subgroup, "value", &l_value));
    ok_int(111, 4, l_value);
    subgroup = config_next_group(subgroup);
    ok(112, subgroup == NULL);
    subgroup = config_find_group(group, "nest");
    ok(113, subgroup != NULL);
    ok_string(114, "nest", config_group_type(subgroup));
    ok_string(115, "1", config_group_tag(subgroup));
    ok(116, config_param_signed_number(subgroup, "param", &l_value));
    ok_int(117, 10, l_value);
    subgroup = config_next_group(subgroup);
    ok(118, subgroup != NULL);
    ok_string(119, "2", config_group_tag(subgroup));
    ok(120, config_param_signed_number(subgroup, "param", &l_value));
    ok_int(121, 10, l_value);
    subgroup = config_next_group(subgroup);
    ok(122, subgroup != NULL);
    ok_string(123, "3", config_group_tag(subgroup));
    ok(124, config_param_signed_number(subgroup, "param", &l_value));
    ok_int(125, 10, l_value);
    subgroup = config_next_group(subgroup);
    ok(126, subgroup != NULL);
    ok_string(127, "4", config_group_tag(subgroup));
    ok(128, config_param_signed_number(subgroup, "param", &l_value));
    ok_int(129, 10, l_value);
    subgroup = config_next_group(subgroup);
    ok(130, subgroup == NULL);
    subgroup = config_find_group(group, "nonexistent");
    ok(131, subgroup == NULL);
    subgroup = config_find_group(group, "nest");
    ok(132, subgroup != NULL);
    subgroup = config_find_group(subgroup, "params");
    ok(133, subgroup != NULL);
    ok_string(134, "params", config_group_type(subgroup));
    ok_string(135, "first", config_group_tag(subgroup));
    ok(136, config_param_signed_number(subgroup, "first", &l_value));
    ok_int(137, 1, l_value);
    ok(138, !config_param_signed_number(subgroup, "second", &l_value));
    subgroup = config_next_group(subgroup);
    ok(139, subgroup != NULL);
    ok_string(140, "second", config_group_tag(subgroup));
    ok(141, config_param_signed_number(subgroup, "first", &l_value));
    ok_int(142, 1, l_value);
    ok(143, config_param_signed_number(subgroup, "second", &l_value));
    ok_int(144, 2, l_value);
    subgroup = config_next_group(subgroup);
    ok(145, subgroup != NULL);
    ok_string(146, "third", config_group_tag(subgroup));
    ok(147, config_param_signed_number(subgroup, "first", &l_value));
    ok_int(148, 1, l_value);
    ok(149, config_param_signed_number(subgroup, "second", &l_value));
    ok_int(150, 2, l_value);
    ok(151, config_param_signed_number(subgroup, "third", &l_value));
    ok_int(152, 3, l_value);
    vector = config_params(subgroup);
    ok(153, vector != NULL);
    ok_int(154, 3, vector->count);
    ok_int(155, 4, vector->allocated);
    ok_string(156, "third", vector->strings[0]);
    ok_string(157, "second", vector->strings[1]);
    ok_string(158, "first", vector->strings[2]);
    vector_free(vector);
    config_free(group);

    /* Includes. */
    group = config_parse_file("config/include");
    ok(159, group != NULL);
    if (group == NULL)
        exit(1);
    subgroup = config_find_group(group, "group");
    ok(160, subgroup != NULL);
    ok(161, config_param_string(subgroup, "foo", &s_value));
    ok_string(162, "baz", s_value);
    ok(163, config_param_string(subgroup, "bar", &s_value));
    ok_string(164, "baz", s_value);
    ok(165, !config_param_signed_number(subgroup, "value", &l_value));
    subgroup = config_next_group(subgroup);
    ok(166, subgroup != NULL);
    subgroup = config_next_group(subgroup);
    ok(167, subgroup != NULL);
    ok_string(168, "test", config_group_tag(subgroup));
    ok(169, config_param_string(subgroup, "foo", &s_value));
    ok_string(170, "baz", s_value);
    ok(171, config_param_signed_number(subgroup, "value", &l_value));
    ok_int(172, 10, l_value);
    config_free(group);

    /* Real numbers. */
    group = config_parse_file("config/reals");
    ok(173, group != NULL);
    ok(174, config_param_real(group, "real1", &d_value));
    ok_double(175, 0.1, d_value);
    ok(176, config_param_real(group, "real2", &d_value));
    ok_double(177, -123.45e10, d_value);
    ok(178, config_param_real(group, "real3", &d_value));
    ok_double(179, 4.0e-3, d_value);
    ok(180, config_param_real(group, "real4", &d_value));
    ok_double(181, 1, d_value);
    config_free(group);

    /* Errors. */
    group = parse_error_config("config/null");
    ok(182, group == NULL);
    ok_string(183, "config/null: invalid NUL character found in file\n",
              errors);
    n = test_errors(184);
    n = test_warnings(n);
    n = test_warnings_bool(n);
    n = test_warnings_int(n);
    n = test_warnings_uint(n);
    n = test_warnings_real(n);
    n = test_warnings_string(n);
    test_warnings_list(n);

    return 0;
}
