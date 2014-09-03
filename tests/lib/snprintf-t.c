/*
 * snprintf test suite.
 *
 * $Id$
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <rra@stanford.edu>
 * Copyright 2000, 2001, 2002, 2003, 2004, 2005, 2006
 *     Russ Allbery <rra@stanford.edu>
 * Copyright 2009, 2010
 *     The Board of Trustees of the Leland Stanford Junior University
 * Copyright 1995 Patrick Powell
 * Copyright 2001 Hrvoje Niksic
 *
 * This code is based on code written by Patrick Powell (papowell@astart.com)
 * It may be used for any purpose as long as this notice remains intact
 * on all source code distributions
 */

#define LIBTEST_NEW_FORMAT 1

#include "config.h"
#include "clibrary.h"

#include "tap/basic.h"

/*
 * Disable the requirement that format strings be literals.  We need variable
 * formats for easy testing.
 */
//#pragma GCC diagnostic ignored "-Wformat-nonliteral"

/*
 * Intentionally don't add the printf attribute here since we pass a
 * zero-length printf format during testing and don't want warnings.
 */
int test_snprintf(char *str, size_t count, const char *fmt, ...);
int test_vsnprintf(char *str, size_t count, const char *fmt, va_list args);

static const char string[] = "abcdefghijklmnopqrstuvwxyz0123456789";

static const char *const fp_formats[] = {
    "%-1.5f",   "%1.5f",    "%31.9f",   "%10.5f",   "% 10.5f",  "%+22.9f",
    "%+4.9f",   "%01.3f",   "%3.1f",    "%3.2f",    "%.0f",     "%.1f",
    "%f",

    /* %e and %g formats aren't really implemented yet. */
#if 0
    "%-1.5e",   "%1.5e",    "%31.9e",   "%10.5e",   "% 10.5e",  "%+22.9e",
    "%+4.9e",   "%01.3e",   "%3.1e",    "%3.2e",    "%.0e",     "%.1e",
    "%e",
    "%-1.5g",   "%1.5g",    "%31.9g",   "%10.5g",   "% 10.5g",  "%+22.9g",
    "%+4.9g",   "%01.3g",   "%3.1g",    "%3.2g",    "%.0g",     "%.1g",
    "%g",
#endif
    NULL
};
static const char *const int_formats[] = {
    "%-1.5d",   "%1.5d",    "%31.9d",   "%5.5d",    "%10.5d",   "% 10.5d",
    "%+22.30d", "%01.3d",   "%4d",      "%d",       "%ld",      NULL
};
static const char *const uint_formats[] = {
    "%-1.5lu",  "%1.5lu",   "%31.9lu",  "%5.5lu",   "%10.5lu",  "% 10.5lu",
    "%+6.30lu", "%01.3lu",  "%4lu",     "%lu",      "%4lx",     "%4lX",
    "%01.3lx",  "%1lo",     NULL
};
static const char *const llong_formats[] = {
    "%lld",     "%-1.5lld",  "%1.5lld",    "%123.9lld",  "%5.5lld",
    "%10.5lld", "% 10.5lld", "%+22.33lld", "%01.3lld",   "%4lld",
    NULL
};
static const char *const ullong_formats[] = {
    "%llu",     "%-1.5llu",  "%1.5llu",    "%123.9llu",  "%5.5llu",
    "%10.5llu", "% 10.5llu", "%+22.33llu", "%01.3llu",   "%4llu",
    "%llx",     "%llo",      NULL
};

static const double fp_nums[] = {
    -1.5, 134.21, 91340.2, 341.1234, 0203.9, 0.96, 0.996, 0.9996, 1.996,
    4.136, 0.1, 0.01, 0.001, 10.1, 0
};
static long int_nums[] = {
    -1, 134, 91340, 341, 0203, 0
};
static unsigned long uint_nums[] = {
    (unsigned long) -1, 134, 91340, 341, 0203, 0
};
static long long llong_nums[] = {
    ~(long long) 0,                     /* All-1 bit pattern. */
    (~(unsigned long long) 0) >> 1,     /* Largest signed long long. */
    -150, 134, 91340, 341,
    0
};
static unsigned long long ullong_nums[] = {
    ~(unsigned long long) 0,            /* All-1 bit pattern. */
    (~(unsigned long long) 0) >> 1,     /* Largest signed long long. */
    134, 91340, 341,
    0
};


static void
test_format(bool trunc, const char *expected, int count,
            const char *format, ...)
{
    char buf[128];
    int result;
    va_list args;

    va_start(args, format);
    result = test_vsnprintf(buf, trunc ? 32 : sizeof(buf), format, args);
    va_end(args);
    is_string(expected, buf, "format %s, wanted %s", format, expected);
    is_int(count, result, "...and output length correct");
}


int
main(void)
{
    int i, count;
    unsigned int j;
    long lcount;
    char lgbuf[128];

    plan(8 +
         (18 + (ARRAY_SIZE(fp_formats) - 1) * ARRAY_SIZE(fp_nums)
          + (ARRAY_SIZE(int_formats) - 1) * ARRAY_SIZE(int_nums)
          + (ARRAY_SIZE(uint_formats) - 1) * ARRAY_SIZE(uint_nums)
          + (ARRAY_SIZE(llong_formats) - 1) * ARRAY_SIZE(llong_nums)
          + (ARRAY_SIZE(ullong_formats) - 1) * ARRAY_SIZE(ullong_nums)) * 2);

    is_int(4, test_snprintf(NULL, 0, "%s", "abcd"), "simple string length");
    is_int(2, test_snprintf(NULL, 0, "%d", 20), "number length");
    is_int(7, test_snprintf(NULL, 0, "Test %.2s", "abcd"), "limited string");
    is_int(1, test_snprintf(NULL, 0, "%c", 'a'), "character length");
    is_int(0, test_snprintf(NULL, 0, ""), "empty format length");

    test_format(true, "abcd", 4, "%s", "abcd");
    test_format(true, "20", 2, "%d", 20);
    test_format(true, "Test ab", 7, "Test %.2s", "abcd");
    test_format(true, "a", 1, "%c", 'a');
    test_format(true, "", 0, "");
    test_format(true, "abcdefghijklmnopqrstuvwxyz01234", 36, "%s", string);
    test_format(true, "abcdefghij", 10, "%.10s", string);
    test_format(true, "  abcdefghij", 12, "%12.10s", string);
    test_format(true, "    abcdefghijklmnopqrstuvwxyz0", 40, "%40s", string);
    test_format(true, "abcdefghij    ", 14, "%-14.10s", string);
    test_format(true, "              abcdefghijklmnopq", 50, "%50s", string);
    test_format(true, "%abcd%", 6, "%%%0s%%", "abcd");
    test_format(true, "", 0, "%.0s", string);
    test_format(true, "abcdefghijklmnopqrstuvwxyz  444", 32, "%.26s  %d",
                string, 4444);
    test_format(true, "abcdefghijklmnopqrstuvwxyz  -2.", 32, "%.26s  %.1f",
                string, -2.5);
    test_format(true, "abcdefghij4444", 14, "%.10s%n%d", string, &count, 4444);
    is_int(10, count, "correct output from %%n");
    test_format(true, "abcdefghijklmnopqrstuvwxyz01234", 36, "%n%s%ln",
                &count, string, &lcount);
    is_int(0, count, "correct output from two %%n");
    is_int(31, lcount, "correct output from long %%ln");
    test_format(true, "(null)", 6, "%s", NULL);

    for (i = 0; fp_formats[i] != NULL; i++)
        for (j = 0; j < ARRAY_SIZE(fp_nums); j++) {
            count = sprintf(lgbuf, fp_formats[i], fp_nums[j]);
            test_format(false, lgbuf, count, fp_formats[i], fp_nums[j]);
        }
    for (i = 0; int_formats[i] != NULL; i++)
        for (j = 0; j < ARRAY_SIZE(int_nums); j++) {
            count = sprintf(lgbuf, int_formats[i], int_nums[j]);
            test_format(false, lgbuf, count, int_formats[i], int_nums[j]);
        }
    for (i = 0; uint_formats[i] != NULL; i++)
        for (j = 0; j < ARRAY_SIZE(uint_nums); j++) {
            count = sprintf(lgbuf, uint_formats[i], uint_nums[j]);
            test_format(false, lgbuf, count, uint_formats[i], uint_nums[j]);
        }
    for (i = 0; llong_formats[i] != NULL; i++)
        for (j = 0; j < ARRAY_SIZE(llong_nums); j++) {
            count = sprintf(lgbuf, llong_formats[i], llong_nums[j]);
            test_format(false, lgbuf, count, llong_formats[i], llong_nums[j]);
        }
    for (i = 0; ullong_formats[i] != NULL; i++)
        for (j = 0; j < ARRAY_SIZE(ullong_nums); j++) {
            count = sprintf(lgbuf, ullong_formats[i], ullong_nums[j]);
            test_format(false, lgbuf, count, ullong_formats[i],
                        ullong_nums[j]);
        }

    return 0;
}
