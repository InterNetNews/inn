/* $Id$ */
/* snprintf test suite. */

#include "config.h"
#include <stdarg.h>
#include <stdio.h>

int test_snprintf(char *str, size_t count, const char *fmt, ...);
int test_vsnprintf(char *str, size_t count, const char *fmt, va_list args);

static const char string[] = "abcdefghijklmnopqrstuvwxyz0123456789";

static const char * const fp_formats[] = {
    "%-1.5f",   "%1.5f",    "%31.9f",   "%10.5f",   "% 10.5f",  "%+22.9f",
    "%+4.9f",   "%01.3f",   "%3.1f",    "%3.2f",    "%.0f",     "%.1f",
    "%f",       NULL
};
static const char * const int_formats[] = {
    "%-1.5d",   "%1.5d",    "%31.9d",   "%5.5d",    "%10.5d",   "% 10.5d",
    "%+22.30d", "%01.3d",   "%4d",      "%d",       "%ld",      NULL
};
static const char * const uint_formats[] = {
    "%-1.5lu",  "%1.5lu",   "%31.9lu",  "%5.5lu",   "%10.5lu",  "% 10.5lu",
    "%+6.30lu", "%01.3lu",  "%4lu",     "%lu",      "%4lx",     "%4lX",
    "%01.3lx",  "%1lo",     NULL
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

#define ARRAY_SIZE(array)       sizeof(array) / sizeof(array[0])

static void
ok_format(int n, const char *expected, int count, const char *format, ...)
{
    char buf[32];
    int result;
    va_list args;

    va_start(args, format);
    result = test_vsnprintf(buf, sizeof(buf), format, args);
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

static void
ok(int n, int success)
{
    printf("%sok %d\n", success ? "" : "not ", n);
}

int
main(void)
{
    int n, i, count;
    unsigned int j;
    long lcount;
    char buf[32];

    printf("%d\n",
           (25 + (ARRAY_SIZE(fp_formats) - 1) * ARRAY_SIZE(fp_nums)
            + (ARRAY_SIZE(int_formats) - 1) * ARRAY_SIZE(int_nums)
            + (ARRAY_SIZE(uint_formats) - 1) * ARRAY_SIZE(uint_nums)));

    ok(1, test_snprintf(NULL, 0, "%s", "abcd") == 4);
    ok(2, test_snprintf(NULL, 0, "%d", 20) == 2);
    ok(3, test_snprintf(NULL, 0, "Test %.2s", "abcd") == 7);
    ok(4, test_snprintf(NULL, 0, "%c", 'a') == 1);
    ok(5, test_snprintf(NULL, 0, "") == 0);

    ok_format(6, "abcd", 4, "%s", "abcd");
    ok_format(7, "20", 2, "%d", 20);
    ok_format(8, "Test ab", 7, "Test %.2s", "abcd");
    ok_format(9, "a", 1, "%c", 'a');
    ok_format(10, "", 0, "");
    ok_format(11, "abcdefghijklmnopqrstuvwxyz01234", 36, "%s", string);
    ok_format(12, "abcdefghij", 10, "%.10s", string);
    ok_format(13, "  abcdefghij", 12, "%12.10s", string);
    ok_format(14, "    abcdefghijklmnopqrstuvwxyz0", 40, "%40s", string);
    ok_format(15, "abcdefghij    ", 14, "%-14.10s", string);
    ok_format(16, "              abcdefghijklmnopq", 50, "%50s", string);
    ok_format(17, "%abcd%", 6, "%%%0s%%", "abcd");
    ok_format(18, "", 0, "%.0s", string);
    ok_format(19, "abcdefghijklmnopqrstuvwxyz  444", 32, "%.26s  %d",
              string, 4444);
    ok_format(20, "abcdefghijklmnopqrstuvwxyz  -2.", 32, "%.26s  %.1f",
              string, -2.5);

    ok_format(21, "abcdefghij4444", 14, "%.10s%n%d", string, &count, 4444);
    ok(22, count == 10);
    ok_format(23, "abcdefghijklmnopqrstuvwxyz01234", 36, "%ln%s%n", &count,
              string, &lcount);
    ok(24, count == 0);
    ok(25, lcount == 31);

    n = 25;
    for (i = 0; fp_formats[i] != NULL; i++)
        for (j = 0; j < ARRAY_SIZE(fp_nums); j++) {
            count = sprintf(buf, fp_formats[i], fp_nums[j]);
            ok_format(++n, buf, count, fp_formats[i], fp_nums[j]);
        }
    for (i = 0; int_formats[i] != NULL; i++)
        for (j = 0; j < ARRAY_SIZE(int_nums); j++) {
            count = sprintf(buf, int_formats[i], int_nums[j]);
            ok_format(++n, buf, count, int_formats[i], int_nums[j]);
        }
    for (i = 0; uint_formats[i] != NULL; i++)
        for (j = 0; j < ARRAY_SIZE(uint_nums); j++) {
            count = sprintf(buf, uint_formats[i], uint_nums[j]);
            ok_format(++n, buf, count, uint_formats[i], uint_nums[j]);
        }

    return 0;
}
