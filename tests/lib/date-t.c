/* $Id$ */
/* makedate test suite */

#include "config.h"
#include "clibrary.h"
#include <time.h>

#include "libinn.h"
#include "libtest.h"

static const time_t test_times[] = {
    28800UL,                    /* Thu,  1 Jan 1970 00:00:00 -0800 (PST) */
    362762400UL,                /* Tue, 30 Jun 1981 15:20:00 +0000 (UTC) */
    396977449UL,                /* Sat, 31 Jul 1982 15:30:49 +0000 (UTC) */
    825597049UL,                /* Thu, 29 Feb 1996 12:30:49 +0000 (UTC) */
    850435199UL,                /* Thu, 12 Dec 1996 23:59:59 +0000 (UTC) */
    852101999UL,                /* Wed,  1 Jan 1997 06:59:59 +0000 (UTC) */
    934288249UL,                /* Tue, 10 Aug 1999 12:30:49 +0000 (UTC) */
    946684800UL,                /* Sat,  1 Jan 2000 00:00:00 +0000 (UTC) */
    946713599UL,                /* Fri, 31 Dec 1999 23:59:59 -0800 (PST) */
    946713600UL,                /* Sat,  1 Jan 2000 00:00:00 -0800 (PST) */
    951827449UL,                /* Tue, 29 Feb 2000 12:30:49 +0000 (UTC) */
    954669599UL,                /* Sun,  2 Apr 2000 01:59:59 -0800 (PST) */
    954669600UL,                /* Sun,  2 Apr 2000 03:00:00 -0700 (PDT) */
    967707668UL,                /* Thu, 31 Aug 2000 07:41:08 +0000 (UTC) */
    972808200UL,                /* Sun, 29 Oct 2000 01:30:00 -0700 (PDT) */
    972809999UL,                /* Sun, 29 Oct 2000 01:59:59 -0700 (PDT) */
    972813600UL                 /* Sun, 29 Oct 2000 02:00:00 -0800 (PST) */
};

static void
ok_time(int n, time_t right, const char *date, const char *hour, bool local)
{
    time_t seen;

    seen = parsedate_nntp(date, hour, local);
    if (right == seen)
        printf("ok %d\n", n);
    else
        printf("not ok %d\n  wanted %lu seen %lu\n  %s %s %d\n", n,
               (unsigned long) right, (unsigned long) seen, date, hour,
               local);
}

static void
check_nntp(int *n, time_t timestamp)
{
    char date[9], hour[7];
    struct tm *tmp_tm, tm;

    tmp_tm = localtime(&timestamp);
    tm = *tmp_tm;
    sprintf(date, "%02d%02d%02d", tm.tm_year % 100, tm.tm_mon + 1,
            tm.tm_mday);
    sprintf(hour, "%02d%02d%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
    ok_time((*n)++, timestamp, date, hour, true);
    sprintf(date, "%04d%02d%02d", tm.tm_year + 1900, tm.tm_mon + 1,
            tm.tm_mday);
    ok_time((*n)++, timestamp, date, hour, true);
    tmp_tm = gmtime(&timestamp);
    tm = *tmp_tm;
    sprintf(date, "%04d%02d%02d", tm.tm_year + 1900, tm.tm_mon + 1,
            tm.tm_mday);
    sprintf(hour, "%02d%02d%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
    ok_time((*n)++, timestamp, date, hour, false);
}

int
main(void)
{
    char buff[64] = "";
    bool status;
    time_t now, result;
    double diff = 0;
    int n;
    unsigned int i;

    char PST8PDT[] = "TZ=PST8PDT";
    char Newfoundland[] = "TZ=Canada/Newfoundland";

    printf("%d\n", 40 + ARRAY_SIZE(test_times) * 3 + 3);

    now = time(NULL);
    status = makedate(-1, false, buff, sizeof(buff));
    if (status) {
        result = parsedate(buff, NULL);
        diff = difftime(result, now);
    }
    ok(1, status && diff >= 0 && diff < 10);
    now = time(NULL);
    status = makedate(-1, true, buff, sizeof(buff));
    if (status) {
        result = parsedate(buff, NULL);
        diff = difftime(result, now);
    }
    ok(2, status && diff >= 0 && diff < 10);

    putenv(PST8PDT);
    tzset();

    status = makedate(100000000UL, false, buff, sizeof(buff));
    ok(3, status);
    ok_string(4, "Sat, 3 Mar 1973 09:46:40 +0000 (UTC)", buff);
    status = makedate(100000000UL, true, buff, sizeof(buff));
    ok(5, status);
    ok_string(6, "Sat, 3 Mar 1973 01:46:40 -0800 (PST)", buff);
    status = makedate(300000000UL, false, buff, sizeof(buff));
    ok(7, status);
    ok_string(8, "Thu, 5 Jul 1979 05:20:00 +0000 (UTC)", buff);
    status = makedate(300000000UL, true, buff, sizeof(buff));
    ok(9, status);
    ok_string(10, "Wed, 4 Jul 1979 22:20:00 -0700 (PDT)", buff);

    status = makedate(300000000UL, false, buff, 31);
    ok(11, !status);
    status = makedate(300000000UL, false, buff, 32);
    ok(12, status);
    ok_string(13, "Thu, 5 Jul 1979 05:20:00 +0000", buff);
    status = makedate(300000000UL, true, buff, 32);
    ok(14, status);
    ok_string(15, "Wed, 4 Jul 1979 22:20:00 -0700", buff);

    putenv(Newfoundland);
    tzset();

    status = makedate(900000045UL, true, buff, sizeof(buff));
    ok(16, status);
    ok_string(17, "Thu, 9 Jul 1998 13:30:45 -0230 (NDT)", buff);

    putenv(PST8PDT);
    tzset();

    ok_time(18, (time_t) -1, "20000132", "000000", false);
    ok_time(19, (time_t) -1, "20000132", "000000", true);
    ok_time(20, (time_t) -1, "20000230", "000000", false);
    ok_time(21, (time_t) -1, "20000230", "000000", true);
    ok_time(22, (time_t) -1, "19990229", "000000", false);
    ok_time(23, (time_t) -1, "19990229", "000000", true);
    ok_time(24, (time_t) -1, "19990020", "000000", false);
    ok_time(25, (time_t) -1, "19990120", "240000", false);
    ok_time(26, (time_t) -1, "19990120", "146000", false);
    ok_time(27, (time_t) -1, "19990120", "145961", false);
    ok_time(28, (time_t) -1,   "691231", "235959", false);
    ok_time(29, (time_t) -1, "19691231", "235959", false);
    ok_time(30, (time_t) -1, "19700100", "000000", false);
    ok_time(31,           0, "19700101", "000000", false);
    ok_time(32,           0,   "700101", "000000", false);
    ok_time(33, (time_t) -1, "2000010101", "000000", false);
    ok_time(34, (time_t) -1,    "00101", "000000", false);
    ok_time(35, (time_t) -1, "20000101",  "11111", false);
    ok_time(36, (time_t) -1, "20000101", "1111111", false);
    ok_time(37, (time_t) -1, "200001a1", "000000", false);
    ok_time(38, (time_t) -1, "20000101", "00a000", false);

    /* Times around the fall daylight savings change are ambiguous; accept
       either of the possible interpretations, but make sure we get one or
       the other. */
    result = parsedate_nntp("20001029", "010000", true);
    ok(39, result == 972806400UL || result == 972810000UL);
    result = parsedate_nntp("001029", "013000", true);
    ok(40, result == 972808200UL || result == 972811800UL);

    n = 41;
    for (i = 0; i < ARRAY_SIZE(test_times); i++)
        check_nntp(&n, test_times[i]);
    check_nntp(&n, time(NULL));

    return 0;
}
