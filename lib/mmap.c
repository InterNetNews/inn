/*  $Id$
**
**  MMap manipulation routines
**
**  Written by Alex Kiernan (alex.kiernan@thus.net)
**
**  These routines work with mmap()ed memory
*/

#include "config.h"
#include "clibrary.h"
#include "portable/mmap.h"

#include "inn/mmap.h"

/*
**  Figure out what page an address is in and flush those pages
*/
void
mapcntl(void *p, size_t length, int flags)
{
    int pagesize;

    pagesize = getpagesize();
    if (pagesize == -1)
        syswarn("getpagesize failed");
    else {
	char *start, *end;

	start = (char *)((size_t)p & ~(size_t)(pagesize - 1));
	end = (char *)((size_t)((char *)p + length + pagesize) &
		       ~(size_t)(pagesize - 1));
	msync(start, end - start, flags);
    }
}
