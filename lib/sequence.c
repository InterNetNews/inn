/*
**  Sequence space arithmetic routines.
**
**  This is a set of routines for implementing so called sequence
**  space arithmetic (typically used for DNS serial numbers). The
**  implementation here is taken from RFC 1982.
*/

#include "config.h"
#include "clibrary.h"
#include <limits.h>
#include "inn/sequence.h"


/*
**  compare two unsigned long numbers using sequence space arithmetic
**
**    returns:
**     0 - i1 = i2
**    -1 - i1 < i2
**     1 - i1 > i2
**     INT_MAX - undefined
*/
int
seq_lcompare(unsigned long i1, unsigned long i2)
{
    if (i1 == i2)
	return 0;
    else if ((i1 < i2 && i2 - i1 < (1 + ULONG_MAX / 2)) ||
	     (i1 > i2 && i1 - i2 > (1 + ULONG_MAX / 2)))
	return -1;
    else if ((i1 < i2 && i2 - i1 > (1 + ULONG_MAX / 2)) ||
	     (i1 > i2 && i1 - i2 < (1 + ULONG_MAX / 2)))
	return 1;
    return INT_MAX;
}
