/* $Id$ */
/* snprintf test suite. */

#include "config.h"
#include <stdarg.h>
#include <stdio.h>

#include "libtest.h"

int test_snprintf(char *str, size_t count, const char *fmt, ...);
int test_vsnprintf(char *str, size_t count, const char *fmt, va_list args);

static const char string[] = "abcdefghijklmnopqrstuvwxyz0123456789";

static const char *const fp_formats[] = {
    "%-1.5f",   "%1.5f",    "%31.9f",   "%10.5f",   "% 10.5f",  "%+22.9f",
    "%+4.9f",   "%01.3f",   "%3.1f",    "%3.2f",    "%.0f",     "%.1f",
    "%f",       NULL
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
    4.136, 0
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
    ~(long long) 0,                     /* All-1 bit pattern. */
    (~(unsigned long long) 0) >> 1,     /* Largest signed long long. */
    134, 91340, 341,
    0
};

#define ARRAY_SIZE(array)       sizeof(array) / sizeof(array[0])

static void
test_format(int n, bool truncate, const char *expected, int count,
            const char *format, ...)
{
    char buf[128];
    int result;
    va_list args;

    va_start(args, format);
    result = test_vsnprintf(buf, truncate ? 32 : sizeof(buf), format, args);
    va_end(args);
    if (!strcmp(buf, expected) && result == count) {
        printf("ok %d\n", n);
    } else {
        printf("not ok %d\n", n);
        printf("  format: %s\n", format);
        if (strcmp(buf, expected))
            printf("   saw: %s\n  want: %s\n", buf, expected);
        if (result != count) printf("  %d != %d\n", result, count);
    }
}

int
main(void)
{
    int n, i, count;
    unsigned int j;
    long lcount;
    char lgbuf[128];

    printf("%d\n",
           (25 + (ARRAY_SIZE(fp_formats) - 1) * ARRAY_SIZE(fp_nums)
            + (ARRAY_SIZE(int_formats) - 1) * ARRAY_SIZE(int_nums)
            + (ARRAY_SIZE(uint_formats) - 1) * ARRAY_SIZE(uint_nums)
            + (ARRAY_SIZE(llong_formats) - 1) * ARRAY_SIZE(llong_nums)
            + (ARRAY_SIZE(ullong_formats) - 1) * ARRAY_SIZE(ullong_nums)));

    ok(1, test_snprintf(NULL, 0, "%s", "abcd") == 4);
    ok(2, test_snprintf(NULL, 0, "%d", 20) == 2);
    ok(3, test_snprintf(NULL, 0, "Test %.2s", "abcd") == 7);
    ok(4, test_snprintf(NULL, 0, "%c", 'a') == 1);
    ok(5, test_snprintf(NULL, 0, "") == 0);

    test_format(6, true, "abcd", 4, "%s", "abcd");
    test_format(7, true, "20", 2, "%d", 20);
    test_format(8, true, "Test ab", 7, "Test %.2s", "abcd");
    test_format(9, true, "a", 1, "%c", 'a');
    test_format(10, true, "", 0, "");
    test_format(11, true, "abcdefghijklmnopqrstuvwxyz01234", 36, "%s",
                string);
    test_format(12, true, "abcdefghij", 10, "%.10s", string);
    test_format(13, true, "  abcdefghij", 12, "%12.10s", string);
    test_format(14, true, "    abcdefghijklmnopqrstuvwxyz0", 40, "%40s",
                string);
    test_format(15, true, "abcdefghij    ", 14, "%-14.10s", string);
    test_format(16, true, "              abcdefghijklmnopq", 50, "%50s",
                string);
    test_format(17, true, "%abcd%", 6, "%%%0s%%", "abcd");
    test_format(18, true, "", 0, "%.0s", string);
    test_format(19, true, "abcdefghijklmnopqrstuvwxyz  444", 32, "%.26s  %d",
                string, 4444);
    test_format(20, true, "abcdefghijklmnopqrstuvwxyz  -2.", 32,
                "%.26s  %.1f", string, -2.5);
    test_format(21, true, "abcdefghij4444", 14, "%.10s%n%d", string, &count,
                4444);
    ok(22, count == 10);
    test_format(23, true, "abcdefghijklmnopqrstuvwxyz01234", 36, "%ln%s%n",
                &count, string, &lcount);
    ok(24, count == 0);
    ok(25, lcount == 31);

    n = 25;
    for (i = 0; fp_formats[i] != NULL; i++)
        for (j = 0; j < ARRAY_SIZE(fp_nums); j++) {
            count = sprintf(lgbuf, fp_formats[i], fp_nums[j]);
            test_format(++n, false, lgbuf, count, fp_formats[i], fp_nums[j]);
        }
    for (i = 0; int_formats[i] != NULL; i++)
        for (j = 0; j < ARRAY_SIZE(int_nums); j++) {
            count = sprintf(lgbuf, int_formats[i], int_nums[j]);
            test_format(++n, false, lgbuf, count, int_formats[i],
                        int_nums[j]);
        }
    for (i = 0; uint_formats[i] != NULL; i++)
        for (j = 0; j < ARRAY_SIZE(uint_nums); j++) {
            count = sprintf(lgbuf, uint_formats[i], uint_nums[j]);
            test_format(++n, false, lgbuf, count, uint_formats[i],
                        uint_nums[j]);
        }
    for (i = 0; llong_formats[i] != NULL; i++)
        for (j = 0; j < ARRAY_SIZE(llong_nums); j++) {
            count = sprintf(lgbuf, llong_formats[i], llong_nums[j]);
            test_format(++n, false, lgbuf, count, llong_formats[i],
                        llong_nums[j]);
        }
    for (i = 0; ullong_formats[i] != NULL; i++)
        for (j = 0; j < ARRAY_SIZE(ullong_nums); j++) {
            count = sprintf(lgbuf, ullong_formats[i], ullong_nums[j]);
            test_format(++n, false, lgbuf, count, ullong_formats[i],
                        ullong_nums[j]);
        }

    return 0;
}
