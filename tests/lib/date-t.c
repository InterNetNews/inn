/* $Id$ */
/* makedate() test suite */

#include "clibrary.h"
#include <time.h>

#include "libinn.h"

static void
ok_string(int n, bool status, const char *wanted, const char *seen)
{
    if (strcmp(wanted, seen) != 0)
        printf("not ok %d\n  wanted: %s\n    seen: %s\n", n, wanted, seen);
    else if (!status)
        printf("not ok %d\n  makedate returned false\n", n);
    else
        printf("ok %d\n", n);
}

static void
ok_bool(int n, bool wanted, bool seen)
{
    if (wanted == seen)
        printf("ok %d\n", n);
    else
        printf("not ok %d\n  wanted %d seen %d\n", wanted, seen);
}

static void
ok(int n, int success)
{
    printf("%sok %d\n", success ? "" : "not ", n);
}

int
main(void)
{
    char buff[64] = "";
    bool status;
    time_t now, result;
    double diff;

    puts("10");

    now = time(NULL);
    status = makedate(0, FALSE, buff, sizeof(buff));
    if (status) {
        result = parsedate(buff, NULL);
        diff = difftime(result, now);
    }
    ok(1, status && diff >= 0 && diff < 10);
    now = time(NULL);
    status = makedate(0, TRUE, buff, sizeof(buff));
    if (status) {
        result = parsedate(buff, NULL);
        diff = difftime(result, now);
    }
    ok(2, status && diff >= 0 && diff < 10);

    putenv("TZ=PST8PDT");

    status = makedate(100000000UL, FALSE, buff, sizeof(buff));
    ok_string(3, status, "Sat, 3 Mar 1973 09:46:40 +0000 (UTC)", buff);
    status = makedate(100000000UL, TRUE, buff, sizeof(buff));
    ok_string(4, status, "Sat, 3 Mar 1973 01:46:40 -0800 (PST)", buff);
    status = makedate(300000000UL, FALSE, buff, sizeof(buff));
    ok_string(5, status, "Thu, 5 Jul 1979 05:20:00 +0000 (UTC)", buff);
    status = makedate(300000000UL, TRUE, buff, sizeof(buff));
    ok_string(6, status, "Wed, 4 Jul 1979 22:20:00 -0700 (PDT)", buff);

    status = makedate(300000000UL, FALSE, buff, 32);
    ok_bool(7, FALSE, status);
    status = makedate(300000000UL, FALSE, buff, 33);
    ok_string(8, status, "Thu, 5 Jul 1979 05:20:00 +0000", buff);
    status = makedate(300000000UL, TRUE, buff, 33);
    ok_string(9, status, "Wed, 4 Jul 1979 22:20:00 -0700", buff);

    putenv("TZ=Canada/Newfoundland");

    status = makedate(900000045UL, TRUE, buff, sizeof(buff));
    ok_string(10, status, "Thu, 9 Jul 1998 13:30:45 -0230 (NDT)", buff);

    return 0;
}
