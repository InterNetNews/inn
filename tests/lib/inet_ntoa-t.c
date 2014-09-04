/* $Id$
 *
 * inet_ntoa test suite.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 *
 * The authors hereby relinquish any claim to any copyright that they may have
 * in this work, whether granted under contract or by operation of law or
 * international treaty, and hereby commit to the public, at large, that they
 * shall not, at any time in the future, seek to enforce any copyright in this
 * work against any person or entity, or prevent any person or entity from
 * copying, publishing, distributing or creating derivative works of this
 * work.
 */

#define LIBTEST_NEW_FORMAT 1

#include "config.h"
#include "clibrary.h"
#include "portable/socket.h"

#include "tap/basic.h"

const char *test_inet_ntoa(const struct in_addr);


static void
test_addr(const char *expected, unsigned long addr)
{
    struct in_addr in;

    in.s_addr = htonl(addr);
    is_string(expected, test_inet_ntoa(in), "address %s", expected);
}


int
main(void)
{
    plan(5);

    test_addr(        "0.0.0.0", 0x0);
    test_addr(      "127.0.0.0", 0x7f000000UL);
    test_addr("255.255.255.255", 0xffffffffUL);
    test_addr("172.200.232.199", 0xacc8e8c7UL);
    test_addr(        "1.2.3.4", 0x01020304UL);

    return 0;
}
