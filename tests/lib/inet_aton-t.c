/* $Id$ */
/* inet_aton test suite. */

#include "config.h"

#include <sys/types.h>
#include <netinet/in.h>
#include <stdio.h>
#if STDC_HEADERS
# include <string.h>
#endif

int test_inet_aton(const char *, struct in_addr *);

static void
ok(int n, int success, const struct in_addr *in, unsigned long addr)
{
    int okay = (success && in->s_addr == htonl(addr));
    
    printf("%sok %d\n", okay ? "" : "not ", n);
    if (!okay && !success) printf("  success: %d\n", success);
    if (!okay && in->s_addr != htonl(addr))
        printf("  want: %lx\n   saw: %lx\n", htonl(addr),
               (unsigned long) in->s_addr);
}

static void
fail(int n, const char *string)
{
    struct in_addr in;
    int success;

    in.s_addr = htonl(0x01020304UL);
    success = test_inet_aton(string, &in);
    success = (success == 0 && in.s_addr == htonl(0x01020304UL));
    printf("%sok %d\n", success ? "" : "not ", n);
}

int
main(void)
{
    struct in_addr in;

    puts("46");

    ok( 1, test_inet_aton(            "0.0.0.0", &in), &in, 0);
    ok( 2, test_inet_aton(     "127.0.0.000000", &in), &in, 0x7f000000UL);
    ok( 3, test_inet_aton(    "255.255.255.255", &in), &in, 0xffffffffUL);
    ok( 4, test_inet_aton(    "172.200.232.199", &in), &in, 0xacc8e8c7UL);
    ok( 5, test_inet_aton(            "1.2.3.4", &in), &in, 0x01020304UL);

    ok( 6, test_inet_aton(    "0x0.0x0.0x0.0x0", &in), &in, 0);
    ok( 7, test_inet_aton("0x7f.0x000.0x0.0x00", &in), &in, 0x7f000000UL);
    ok( 8, test_inet_aton("0xff.0xFf.0xFF.0xff", &in), &in, 0xffffffffUL);
    ok( 9, test_inet_aton("0xAC.0xc8.0xe8.0xC7", &in), &in, 0xacc8e8c7UL);
    ok(10, test_inet_aton("0xAa.0xbB.0xCc.0xdD", &in), &in, 0xaabbccddUL);
    ok(11, test_inet_aton("0xEe.0xfF.0.0x00000", &in), &in, 0xeeff0000UL);
    ok(12, test_inet_aton("0x1.0x2.0x00003.0x4", &in), &in, 0x01020304UL);

    ok(13, test_inet_aton(   "000000.00.000.00", &in), &in, 0);
    ok(14, test_inet_aton(             "0177.0", &in), &in, 0x7f000000UL);
    ok(15, test_inet_aton("0377.0377.0377.0377", &in), &in, 0xffffffffUL);
    ok(16, test_inet_aton("0254.0310.0350.0307", &in), &in, 0xacc8e8c7UL);
    ok(17, test_inet_aton("00001.02.3.00000004", &in), &in, 0x01020304UL);

    ok(18, test_inet_aton(           "16909060", &in), &in, 0x01020304UL);
    ok(19, test_inet_aton(      "172.062164307", &in), &in, 0xacc8e8c7UL);
    ok(20, test_inet_aton(    "172.0xc8.0xe8c7", &in), &in, 0xacc8e8c7UL);
    ok(21, test_inet_aton(              "127.1", &in), &in, 0x7f000001UL);
    ok(22, test_inet_aton(         "0xffffffff", &in), &in, 0xffffffffUL);
    ok(23, test_inet_aton(       "127.0xffffff", &in), &in, 0x7fffffffUL);
    ok(24, test_inet_aton(     "127.127.0xffff", &in), &in, 0x7f7fffffUL);

    fail(25,                  "");
    fail(26,      "Donald Duck!");
    fail(27,        "a127.0.0.1");
    fail(28,          "aaaabbbb");
    fail(29,       "0x100000000");
    fail(30,       "0xfffffffff");
    fail(31,     "127.0xfffffff");
    fail(32,     "127.376926742");
    fail(33,  "127.127.01452466");
    fail(34, "127.127.127.0x100");
    fail(35,             "256.0");
    fail(36,  "127.0378.127.127");
    fail(37, "127.127.0x100.127");
    fail(38,         "127.0.o.1");
    fail(39,  "127.127.127.127v");
    fail(40,    "ef.127.127.127");
    fail(41,  "0128.127.127.127");
    fail(42,          "0xeg.127");
    fail(43,          ".127.127");
    fail(44,          "127.127.");
    fail(45,          "127..127");
    fail(46,       "de.ad.be.ef");

    return 0;
}
