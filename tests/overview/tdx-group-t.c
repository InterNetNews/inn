/*
**  Test suite for tradindexed group index size validation.
**
**  Written by Kevin Bowling in 2026.
*/

#define LIBTEST_NEW_FORMAT 1

#include "portable/system.h"

#include <errno.h>
#include <limits.h>

#include "tap/basic.h"

#include "../../storage/tradindexed/tdx-private.h"
#include "../../storage/tradindexed/tdx-structure.h"

int
main(void)
{
    uintmax_t bytes;
    off_t size;
    bool valid;
    int count, oerrno;

    bytes = sizeof(struct group_header)
            + ((uintmax_t) INT_MAX + 1) * sizeof(struct group_entry);
    size = (off_t) bytes;
    if (size < 0 || (uintmax_t) size != bytes)
        skip_all("off_t cannot represent the oversized test index");

    plan(4);
    ok(tdx_index_entry_count(sizeof(struct group_header), &count),
       "accept an empty index");
    is_int(0, count, "empty index has no entries");
    errno = 0;
    valid = tdx_index_entry_count(size, &count);
    oerrno = errno;
    ok(!valid, "reject an index with more than INT_MAX entries");
    is_int(EOVERFLOW, oerrno, "oversized entry count sets errno");
    return 0;
}
