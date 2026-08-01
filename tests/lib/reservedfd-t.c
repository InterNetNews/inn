/*  Test suite for reserved stdio file descriptors.
**
**  Written by Kevin Bowling in 2026.
*/

#include "portable/system.h"

#include "inn/libinn.h"
#include "tap/basic.h"

int
main(void)
{
    FILE *file;

    test_init(11);

    ok(1, fdreserve(4));
    ok(2, fdreserve(2));
    ok(3, fdreserve(4));

    file = Fopen("/dev/null", "r", 3);
    ok(4, file != NULL);
    ok_int(5, 0, Fclose(file));

    /* The upper bound is exclusive and must fall back to ordinary fopen. */
    file = Fopen("/dev/null", "r", 4);
    ok(6, file != NULL);
    ok_int(7, 0, Fclose(file));

    /* A failed freopen closes its stream; the reserved slot must be rebuilt
       with fopen before it can be reused. */
    file = Fopen("/this/path/does/not/exist", "r", 0);
    ok(8, file == NULL);
    file = Fopen("/dev/null", "r", 0);
    ok(9, file != NULL);
    ok_int(10, 0, Fclose(file));

    ok(11, fdreserve(0));
    return 0;
}
