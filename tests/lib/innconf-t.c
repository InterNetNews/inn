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

    if (access("config/valid", F_OK) < 0)
        if (access("lib/config/valid", F_OK) == 0)
            chdir("lib");

    puts("4");

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
    ok(4, true);

    return 0;
}
