/* $Id$ */
/* makedate test suite */

#include "config.h"
#include "clibrary.h"
#include <time.h>

#include "inn/libinn.h"
#include "tap/basic.h"

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
    972813600UL                 /* Sun, 29 Oct 2000 02:00:00 -0800 (PST) */
};

/* Used to hold dates for testing parsedate_rfc5322. */
struct test_date {
    const char *date;
    time_t result;
};

static const struct test_date test_dates[] = {
    { "Thu, 20 May 2004 23:43:29 +0000 (UTC)",   1085096609UL },
    { "FRI, 21 MAY 2004 11:19:57 GMT",           1085138397UL },
    { "Fri, 21 May 2004 11:19:57 UT",            1085138397UL },
    { "Fri, 21 May 2004 13:16:47 +0100",         1085141807UL },
    { "fri, 21 may 2004 12:36:08 pdt",           1085168168ul },
    { "Fri, 21 May 2004 12:36:08 MDT",           1085164568UL },
    { "Fri, 21 May 2004 12:36:08 CdT",           1085160968UL },
    { "Fri, 21 May 2004 12:36:08 EDT",           1085157368UL },
    { "Sun, 23 Jan 2004 23:54:30 PST",           1074930870UL },
    { "Sun, 23 Jan 2004 23:54:30 MST",           1074927270UL },
    { "Sun, 23 Jan 2004 23:54:30 CST",           1074923670UL },
    { "Sun, 23 Jan 2004 23:54:30 EST",           1074920070UL },
    { "26 May 2004 15:13:32 +0100",              1085580812UL },
    { "wed, 26 may 2004 15:27:15 b",             1085585235ul },
    { "26 May 2004 15:27:15 Z",                  1085585235UL },
    { "7 May 2004 22:54:39 +0000 (UTC)",         1083970479UL },
    { " 7 May 2004 22:54:39 +0000 (UTC)",        1083970479UL },
    { "Fri, 7 May 2004 22:54:39 +0000 (UTC)",    1083970479UL },
    { "FrI,  7 MaY 2004 22:54:39 +0000 (UTC)",   1083970479UL },
    { "(foo)Fri(bar),28May200407(baz):    \r\n (bar)11\r\n\t:\t32(baz)+0100",
                                                 1085724692UL },
    { "Fri, 28 May 04 11:01:32 +0100",           1085738492UL },
    { "Fri, 28 May 104 11:01:32 +0100",          1085738492UL },
    { "28 May 99 11:01:32 D",                     927889292UL }
};

static const struct test_date test_dates_lax[] = {
    { "Wednesday, 26 May 2004 12:16:44 -0600",   1085595404UL },
    { "Sat, 01 June 2004 12:23:19 -0400",        1086106999UL },
    { "Sat, 01 Jun. 2004 12:23:19 -0400",        1086106999UL },
    { "6 Jun 2004 6:4:6 +0100",                  1086498246UL },
    { "1 Jun 2004 06:02:22 GMT+0000",            1086069742UL },
    { "1 Jun 2004 06:02:22 GMT+0100",            1086066142UL },
    { "Wed, 2 Jun 2004 01:40:16 GMT+2",          1086133216UL },
    { "Mon, 17 May 2004 18:32:06 +02120 (MEST)", 1084741926UL },
    { "8 Feb 98 00:49:48 -500",                   886916988UL },
    { "Tue, 25 May 2004 22:14:23 BST",           1085519663UL },
    { "Tue, 23 Feb 1993 13:40:33 EET",            730467633UL },
    { "21 Apr 1998 15:42:04",                     893198524UL },
    { "Sat, 22 May 2004 23:41:27 -0700 EST",     1085287287UL },
    { "21 Apr 1998 15:42:04 and other bits",      893173324UL }
};

static void
ok_time(int n, time_t wanted, time_t seen)
{
    if (wanted == seen)
        ok(n, true);
    else {
        ok(n, false);
        diag("wanted %lu seen %lu\n",
             (unsigned long) wanted, (unsigned long) seen);
    }
}

static void
ok_nntp(int n, time_t right, const char *date, const char *hour, bool local)
{
    time_t seen;

    seen = parsedate_nntp(date, hour, local);
    ok_time(n, right, seen);
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
    ok_nntp((*n)++, timestamp, date, hour, true);
    sprintf(date, "%04d%02d%02d", tm.tm_year + 1900, tm.tm_mon + 1,
            tm.tm_mday);
    ok_nntp((*n)++, timestamp, date, hour, true);
    tmp_tm = gmtime(&timestamp);
    tm = *tmp_tm;
    sprintf(date, "%04d%02d%02d", tm.tm_year + 1900, tm.tm_mon + 1,
            tm.tm_mday);
    sprintf(hour, "%02d%02d%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
    ok_nntp((*n)++, timestamp, date, hour, false);
}

static void
check_strict(int *n, const struct test_date *test)
{
    time_t seen;

    seen = parsedate_rfc5322(test->date);
    ok_time((*n)++, test->result, seen);
}

static void
check_strict_bad(int *n, const struct test_date *test)
{
    time_t seen;

    seen = parsedate_rfc5322(test->date);
    ok_time((*n)++, -1, seen);
}

static void
check_lax(int *n, const struct test_date *test)
{
    time_t seen;

    seen = parsedate_rfc5322_lax(test->date);
    ok_time((*n)++, test->result, seen);
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

    test_init(44
              + ARRAY_SIZE(test_times) * 3 + 3
              + ARRAY_SIZE(test_dates) * 2
              + ARRAY_SIZE(test_dates_lax) * 2);

    now = time(NULL);
    status = makedate(-1, false, buff, sizeof(buff));
    if (status) {
        result = parsedate_rfc5322(buff);
        diff = difftime(result, now);
    }
    ok(1, status && diff >= 0 && diff < 10);
    now = time(NULL);
    status = makedate(-1, true, buff, sizeof(buff));
    if (status) {
        result = parsedate_rfc5322(buff);
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
    if (memcmp(buff, "Thu, 9 Jul 1998 16:00:45 +0000", 30) == 0)
        skip(17, "Newfoundland time zone not installed");
    else
        ok_string(17, "Thu, 9 Jul 1998 13:30:45 -0230 (NDT)", buff);

    putenv(PST8PDT);
    tzset();

    ok_nntp(18, (time_t) -1, "20000132", "000000", false);
    ok_nntp(19, (time_t) -1, "20000132", "000000", true);
    ok_nntp(20, (time_t) -1, "20000230", "000000", false);
    ok_nntp(21, (time_t) -1, "20000230", "000000", true);
    ok_nntp(22, (time_t) -1, "19990229", "000000", false);
    ok_nntp(23, (time_t) -1, "19990229", "000000", true);
    ok_nntp(24, (time_t) -1, "19990020", "000000", false);
    ok_nntp(25, (time_t) -1, "19990120", "240000", false);
    ok_nntp(26, (time_t) -1, "19990120", "146000", false);
    ok_nntp(27, (time_t) -1, "19990120", "145961", false);
    ok_nntp(28, (time_t) -1,   "691231", "235959", false);
    ok_nntp(29, (time_t) -1, "19691231", "235959", false);
    ok_nntp(30, (time_t) -1, "19700100", "000000", false);
    ok_nntp(31,           0, "19700101", "000000", false);
    ok_nntp(32,           0,   "700101", "000000", false);
    ok_nntp(33, (time_t) -1, "2000010101", "000000", false);
    ok_nntp(34, (time_t) -1,    "00101", "000000", false);
    ok_nntp(35, (time_t) -1, "20000101",  "11111", false);
    ok_nntp(36, (time_t) -1, "20000101", "1111111", false);
    ok_nntp(37, (time_t) -1, "200001a1", "000000", false);
    ok_nntp(38, (time_t) -1, "20000101", "00a000", false);

    /* Times around the fall daylight savings change are ambiguous; accept
       either of the possible interpretations, but make sure we get one or
       the other. */
    result = parsedate_nntp("20001029", "010000", true);
    ok(39, result == 972806400UL || result == 972810000UL);
    result = parsedate_nntp("001029", "010000", true);
    ok(40, result == 972806400UL || result == 972810000UL);
    result = parsedate_nntp("20001029", "013000", true);
    ok(41, result == 972808200UL || result == 972811800UL);
    result = parsedate_nntp("001029", "013000", true);
    ok(42, result == 972808200UL || result == 972811800UL);
    result = parsedate_nntp("20001029", "015959", true);
    ok(43, result == 972809999UL || result == 972813599UL);
    result = parsedate_nntp("001029", "015959", true);
    ok(44, result == 972809999UL || result == 972813599UL);

    n = 45;
    for (i = 0; i < ARRAY_SIZE(test_times); i++)
        check_nntp(&n, test_times[i]);
    check_nntp(&n, time(NULL));
    for (i = 0; i < ARRAY_SIZE(test_dates); i++)
        check_strict(&n, &test_dates[i]);
    for (i = 0; i < ARRAY_SIZE(test_dates_lax); i++)
        check_strict_bad(&n, &test_dates_lax[i]);
    for (i = 0; i < ARRAY_SIZE(test_dates); i++)
        check_lax(&n, &test_dates[i]);
    for (i = 0; i < ARRAY_SIZE(test_dates_lax); i++)
        check_lax(&n, &test_dates_lax[i]);

    return 0;
}
