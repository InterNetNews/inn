/*  $Id$
**
**  Manipulation routines for memory-mapped pages.
**
**  Written by Alex Kiernan (alex.kiernan@thus.net)
*/

#include "config.h"
#include "clibrary.h"
#include "portable/mmap.h"

#include "inn/messages.h"
#include "inn/mmap.h"

/*
**  Figure out what page an address is in and call msync on the appropriate
**  page.  This routine assumes that all pointers fit into a size_t.
*/
void
msync_page(void *p, size_t length, int flags)
{
    int pagesize;

    pagesize = getpagesize();
    if (pagesize == -1)
        syswarn("getpagesize failed");
    else {
        const size_t mask = ~(size_t)(pagesize - 1);
        char *start = (char *) ((size_t) p & mask);
        char *end = (char *) (((size_t) p + length + pagesize) & mask);

        msync(start, end - start, flags);
    }
}
