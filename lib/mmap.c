/*
**  Manipulation routines for memory-mapped pages.
**
**  Written by Alex Kiernan <alex.kiernan@thus.net> in 2002.
**
**  Various bug fixes, code and documentation improvements
**  in 2002-2004, 2007, 2021-2023, 2025.
*/

#include "portable/system.h"

#include "portable/mmap.h"

#include "inn/messages.h"
#include "inn/mmap.h"

/*
**  Figure out what page an address is in and call msync on the appropriate
**  page.  This routine assumes that all pointers fit into a size_t.
*/
int
inn__msync_page(void *p, size_t length, int flags)
{
    int pagesize;

    pagesize = getpagesize();
    if (pagesize == -1) {
        syswarn("getpagesize failed");
        return -1;
    } else {
        const size_t mask = ~(size_t) (pagesize - 1);
        char *start = (char *) ((uintptr_t) p & mask);
        char *end = (char *) (((uintptr_t) p + length + pagesize - 1) & mask);

        return msync(start, end - start, flags);
    }
}
