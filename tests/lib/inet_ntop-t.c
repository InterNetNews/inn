/* $Id$ */
/* inet_ntop test suite. */

#include "config.h"
#include "clibrary.h"
#include "portable/socket.h"
#include <errno.h>

#include "libtest.h"

#ifndef EAFNOSUPPORT
# define EAFNOSUPPORT EDOM
#endif

const char *test_inet_ntop(int, const void *, char *, socklen_t);

static int
test_addr(int n, const char *expected, unsigned long addr)
{
    struct in_addr in;
    char result[INET_ADDRSTRLEN];

    in.s_addr = htonl(addr);
    if (test_inet_ntop(AF_INET, &in, result, sizeof(result)) == NULL) {
        ok(n++, false);
        printf("# cannot convert %lu: %s", addr, strerror(errno));
    } else
        ok(n++, true);
    ok_string(n++, expected, result);
    return n;
}

int
main(void)
{
    int n;

    test_init(6 + 5 * 2);

    ok(1, test_inet_ntop(AF_UNIX, NULL, NULL, 0) == NULL);
    ok_int(2, EAFNOSUPPORT, errno);
    ok(3, test_inet_ntop(AF_INET, NULL, NULL, 0) == NULL);
    ok_int(4, ENOSPC, errno);
    ok(5, test_inet_ntop(AF_INET, NULL, NULL, 11) == NULL);
    ok_int(6, ENOSPC, errno);

    n = 7;
    n = test_addr(n,         "0.0.0.0", 0x0);
    n = test_addr(n,       "127.0.0.0", 0x7f000000UL);
    n = test_addr(n, "255.255.255.255", 0xffffffffUL);
    n = test_addr(n, "172.200.232.199", 0xacc8e8c7UL);
    n = test_addr(n,         "1.2.3.4", 0x01020304UL);

    return 0;
}
