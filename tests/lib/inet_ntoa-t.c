/* $Id$ */
/* inet_ntoa test suite. */

#include "config.h"

#include <sys/types.h>
#include <netinet/in.h>
#include <stdio.h>
#if STDC_HEADERS
# include <string.h>
#endif

const char *test_inet_ntoa(const struct in_addr);

static void
ok(int n, int success)
{
    printf("%sok %d\n", success ? "" : "not ", n);
}

int
main(void)
{
    struct in_addr in;

    puts("5");
    in.s_addr = 0x0;
    ok(1, !strcmp(test_inet_ntoa(in), "0.0.0.0"));
    in.s_addr = htonl(0x7f000000UL);
    ok(2, !strcmp(test_inet_ntoa(in), "127.0.0.0"));
    in.s_addr = htonl(0xffffffffUL);
    ok(3, !strcmp(test_inet_ntoa(in), "255.255.255.255"));
    in.s_addr = htonl(0xacc8e8c7UL);
    ok(4, !strcmp(test_inet_ntoa(in), "172.200.232.199"));
    in.s_addr = htonl(0x01020304UL);
    ok(5, !strcmp(test_inet_ntoa(in), "1.2.3.4"));
    return 0;
}
