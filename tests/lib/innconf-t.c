/* $Id$ */
/* innconf test suite. */

#include "config.h"
#include "clibrary.h"

#include "inn/innconf.h"
#include "inn/messages.h"
#include "libtest.h"

static const char grep[] =
"egrep 'mta|organization|ovmethod|hismethod|path|pgpverify'\
 ../../samples/inn.conf > config/tmp";

int
main(void)
{
    struct innconf *standard;
    FILE *config;

    if (access("../data/config/valid", F_OK) == 0)
        chdir("../data");
    else if (access("data/config/valid", F_OK) == 0)
        chdir("data");
    else if (access("tests/data/config/valid", F_OK) == 0)
        chdir("tests/data");

    test_init(9);

    ok(1, innconf_read("../../samples/inn.conf"));
    standard = innconf;
    innconf = NULL;
    if (system(grep) != 0)
        die("Unable to create stripped configuration file");
    ok(2, innconf_read("config/tmp"));
    unlink("config/tmp");
    ok(3, innconf_compare(standard, innconf));
    innconf_free(standard);
    innconf_free(innconf);
    innconf = NULL;
    ok(4, true);

    /* Checking inn.conf. */
    errors_capture();
    if (system(grep) != 0)
        die("Unable to create stripped configuration file");
    ok(5, innconf_check("config/tmp"));
    ok(6, errors == NULL);
    innconf_free(innconf);
    innconf = NULL;
    config = fopen("config/tmp", "a");
    if (config == NULL)
        sysdie("Unable to open stripped configuration file for append");
    fputs("foo: bar\n", config);
    fclose(config);
    ok(7, !innconf_check("config/tmp"));
    unlink("config/tmp");
    ok_string(8, "config/tmp:26: unknown parameter foo\n", errors);
    errors_uncapture();
    free(errors);
    errors = NULL;
    innconf_free(innconf);
    innconf = NULL;
    ok(9, true);

    return 0;
}
