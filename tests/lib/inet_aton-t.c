/* $Id$ */
/* inet_aton test suite. */

#include "config.h"
#include "clibrary.h"
#include <netinet/in.h>

#include "libtest.h"

int test_inet_aton(const char *, struct in_addr *);

static void
test_addr(int n, const char *string, unsigned long addr)
{
    bool success, okay;
    struct in_addr in;

    success = test_inet_aton(string, &in);
    okay = (success && in.s_addr == htonl(addr));
    
    printf("%sok %d\n", okay ? "" : "not ", n);
    if (!okay && !success) printf("  success: %d\n", success);
    if (!okay && in.s_addr != htonl(addr))
        printf("  want: %lx\n   saw: %lx\n", (unsigned long) htonl(addr),
               (unsigned long) in.s_addr);
}

static void
test_fail(int n, const char *string)
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
    test_init(46);

    test_addr( 1,             "0.0.0.0", 0);
    test_addr( 2,      "127.0.0.000000", 0x7f000000UL);
    test_addr( 3,     "255.255.255.255", 0xffffffffUL);
    test_addr( 4,     "172.200.232.199", 0xacc8e8c7UL);
    test_addr( 5,             "1.2.3.4", 0x01020304UL);

    test_addr( 6,     "0x0.0x0.0x0.0x0", 0);
    test_addr( 7, "0x7f.0x000.0x0.0x00", 0x7f000000UL);
    test_addr( 8, "0xff.0xFf.0xFF.0xff", 0xffffffffUL);
    test_addr( 9, "0xAC.0xc8.0xe8.0xC7", 0xacc8e8c7UL);
    test_addr(10, "0xAa.0xbB.0xCc.0xdD", 0xaabbccddUL);
    test_addr(11, "0xEe.0xfF.0.0x00000", 0xeeff0000UL);
    test_addr(12, "0x1.0x2.0x00003.0x4", 0x01020304UL);

    test_addr(13,    "000000.00.000.00", 0);
    test_addr(14,              "0177.0", 0x7f000000UL);
    test_addr(15, "0377.0377.0377.0377", 0xffffffffUL);
    test_addr(16, "0254.0310.0350.0307", 0xacc8e8c7UL);
    test_addr(17, "00001.02.3.00000004", 0x01020304UL);

    test_addr(18,            "16909060", 0x01020304UL);
    test_addr(19,       "172.062164307", 0xacc8e8c7UL);
    test_addr(20,     "172.0xc8.0xe8c7", 0xacc8e8c7UL);
    test_addr(21,               "127.1", 0x7f000001UL);
    test_addr(22,          "0xffffffff", 0xffffffffUL);
    test_addr(23,        "127.0xffffff", 0x7fffffffUL);
    test_addr(24,      "127.127.0xffff", 0x7f7fffffUL);

    test_fail(25,                  "");
    test_fail(26,      "Donald Duck!");
    test_fail(27,        "a127.0.0.1");
    test_fail(28,          "aaaabbbb");
    test_fail(29,       "0x100000000");
    test_fail(30,       "0xfffffffff");
    test_fail(31,     "127.0xfffffff");
    test_fail(32,     "127.376926742");
    test_fail(33,  "127.127.01452466");
    test_fail(34, "127.127.127.0x100");
    test_fail(35,             "256.0");
    test_fail(36,  "127.0378.127.127");
    test_fail(37, "127.127.0x100.127");
    test_fail(38,         "127.0.o.1");
    test_fail(39,  "127.127.127.127v");
    test_fail(40,    "ef.127.127.127");
    test_fail(41,  "0128.127.127.127");
    test_fail(42,          "0xeg.127");
    test_fail(43,          ".127.127");
    test_fail(44,          "127.127.");
    test_fail(45,          "127..127");
    test_fail(46,       "de.ad.be.ef");

    return 0;
}
