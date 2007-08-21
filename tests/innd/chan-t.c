/* $Id$ */
/* Test suite for general channel handling. */

#include "config.h"
#include "clibrary.h"
#include <fcntl.h>
#include <sys/time.h>

#include "inn/buffer.h"
#include "inn/innconf.h"
#include "inn/libinn.h"
#include "libtest.h"

#include "../../innd/innd.h"

/* Prototypes. */
static void reader    (CHANNEL *cp UNUSED);
static void writedone (CHANNEL *cp UNUSED);
static void wakeup    (CHANNEL *cp UNUSED);

/* Initialize things enough to be able to call channel functions.  This only
   has to be called once. */
static void
initialize(void)
{
    if (access("../data/etc/inn.conf", F_OK) < 0)
        if (access("data/etc/inn.conf", F_OK) == 0)
            if (chdir("innd") != 0)
                sysdie("cannot cd to innd");
    if (!innconf_read("../data/etc/inn.conf"))
        exit(1);
    Log = fopen("/dev/null", "w");
    if (Log == NULL)
        sysdie("cannot open /dev/null");
    CHANsetup(32);
    gettimeofday(&Now, NULL);
}

/* This just gets things started.  Then we call CHANreadloop, which should
   dispatch control to writedone, where we create a new channel to read what
   we just wrote and go to sleep.  The reader function verifies the data was
   written out correctly and then wakes up the first channel to test the sleep
   and wakeup functionality. */
int
main(void)
{
    int fd;
    CHANNEL *cp;

    test_init(15);
    initialize();
    message_handlers_notice(0);

    /* If we lose on waking up a sleeping channel, this will finish off
       matters before we wake up anyway. */
    alarm(5);

    fd = open("output", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0)
        sysdie("cannot create output");
    cp = CHANcreate(fd, CTany, CSwriting, reader, writedone);
    ok_int(1, fd, cp->fd);
    ok_int(2, CTany, cp->Type);
    ok_int(3, CSwriting, cp->State);
    ok(4, !CHANsleeping(cp));
    WCHANset(cp, "some output", strlen("some output"));
    WCHANadd(cp);
    CHANreadloop();
    die("fell through main");
    return 1;
}

static void
writedone(CHANNEL *cp)
{
    CHANNEL *newcp;
    int fd;
    time_t now;

    ok(5, true);
    ok_int(6, cp->Out.left, 0);
    close(cp->fd);
    cp->fd = -1;
    fd = open("output", O_RDONLY | O_EXCL);
    if (fd < 0)
        sysdie("cannot open output");
    newcp = CHANcreate(fd, CTany, CSgetbody, reader, NULL);
    RCHANadd(newcp);
    ok(7, !CHANsleeping(newcp));
    now = time(NULL);
    SCHANadd(cp, now + 60, &Now, wakeup, xstrdup("some random text"));
    ok(8, CHANsleeping(cp));
}

static void
reader(CHANNEL *cp)
{
    ok(9, true);
    ok_int(10, strlen("some output"), CHANreadtext(cp));
    ok_int(11, strlen("some output"), cp->In.used);
    /* FIXME: As long as the In buffer does not use the normal meanings of
       .used and .left, we cannot use buffer_append.
       buffer_append(&cp->In, "", 1); */
    memcpy(cp->In.data + cp->In.used, "", 1);
    ok_string(12, "some output", cp->In.data);
    SCHANwakeup(&Now);
}

static void
wakeup(CHANNEL *cp)
{
    ok(13, true);
    ok_string(14, "some random text", cp->Argument);
    CHANclose(cp, CHANname(cp));
    ok_int(15, CTfree, cp->Type);
    CHANshutdown();
    unlink("output");
    exit(0);
}
