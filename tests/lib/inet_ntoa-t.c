/* $Id$ */
/* inet_ntoa test suite. */

#include "config.h"
#include "clibrary.h"
#include <netinet/in.h>

#include "libtest.h"

const char *test_inet_ntoa(const struct in_addr);

static void
test_addr(int n, const char *expected, unsigned long addr)
{
    struct in_addr in;

    in.s_addr = htonl(addr);
    ok_string(n, expected, test_inet_ntoa(in));
}

int
main(void)
{
    test_init(5);

    test_addr(1,         "0.0.0.0", 0x0);
    test_addr(2,       "127.0.0.0", 0x7f000000UL);
    test_addr(3, "255.255.255.255", 0xffffffffUL);
    test_addr(4, "172.200.232.199", 0xacc8e8c7UL);
    test_addr(5,         "1.2.3.4", 0x01020304UL);

    return 0;
}
