/* $Id$ */
/* confparse test suite. */

#include "config.h"
#include "clibrary.h"
#include <unistd.h>

#include "inn/confparse.h"
#include "libinn.h"

static void
ok(int n, int success)
{
    printf("%sok %d\n", success ? "" : "not ", n);
}

static void
ok_string(int n, const char *saw, const char *expected)
{
    if (strcmp(saw, expected) == 0)
        printf("ok %d\n", n);
    else
        printf("not ok %d\n  saw: %s\n  not: %s\n", n, saw, expected);
}

int
main(void)
{
    config_group group;
    bool b_value = false;
    long l_value = 1;
    const char *s_value;

    puts("39");

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
    return 0;
}
