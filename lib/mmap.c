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
#include <assert.h>

#include "inn/mmap.h"

#include "portable/mmap.h"

static ssize_t pagesize = -1;

/*
**  Return the system pagesize
*/
ssize_t
syspagesize(void)
{
    if (pagesize == -1) {
#if defined(HAVE_GETPAGESIZE)
	pagesize = getpagesize();
#elif defined(_SC_PAGESIZE)
	pagesize = sysconf(_SC_PAGESIZE);
	if (pagesize == -1) {
	    syswarn("sysconf(_SC_PAGESIZE) failed: %m");
	}
#else
	pagesize = 16384;
#endif
    }
    return pagesize;
}

/*
**  Figure out what page an address is in and flush those pages
*/
void
mapcntl(void *p, size_t length, int flags)
{
    if (syspagesize() != -1) {
	char *start, *end;

	start = (char *)((size_t)p & ~(size_t)(pagesize - 1));
	end = (char *)((size_t)((char *)p + length + pagesize) &
		       ~(size_t)(pagesize - 1));
	msync(start, end - start, flags);
    }
}
