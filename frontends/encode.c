/*
**  Produce a seven-bit printable encoding of stdin on stdout.
**  From @(#)encode.c 1.3 5/15/85, distributed with B2.11 News.
**
**  Various bug fixes, code and documentation improvements since then
**  in 1997-2000, 2002, 2003, 2021, 2024.
**
**  The encoding uses characters from 0x20 (' ') through 0x7A ('z').
**  (That fits nicely into the UUCP 'f' protocol by Piet Beertema.) First,
**  expand three eight-bit characters into four six-bit ones.  Collect
**  until we have 13, and spread the last one over the first 12, so that
**  we have 12 6.5-bit characters.  Since there are very few half-bit
**  machines, collect them into pairs, making six 13-bit characters.  We
**  can do this as A * 91 + B where A and B are less than 91 after we add
**  0x20 to make it printable.
**
**  And if you thought that was unclear, then we won't even get into the
**  terminating conditions!
*/

#include "portable/system.h"

/*
**  These characters can't appear in normal output, so we use them to
**  mark that the data that follows is the terminator.  The character
**  immediately following this pair is the length of the terminator (which
**  otherwise might be indeterminable)
*/
#define ENDMARK1 ((90 * 91 + 90) / 91 + ' ')
#define ENDMARK2 ((90 * 91 + 90) % 91 + ' ')

static char Buffer[13];
static int Count;


static void
dumpcode(char *p, int n)
{
    int last;
    int c;

    if (n == 13) {
        n--;
        last = p[12];
    } else if (n & 1)
        last = 1 << (6 - 1);
    else
        last = 0;

    for (; n > 0; n -= 2) {
        c = *p++ << 6;
        c |= *p++;
        if (last & (1 << (6 - 1)))
            c |= (1 << 12);
        last <<= 1;

        putchar((c / 91) + ' ');
        putchar((c % 91) + ' ');
    }
}

static void
flushout(void)
{
    putchar(ENDMARK1);
    putchar(ENDMARK2);
    putchar(Count + ' ');
    dumpcode(Buffer, Count);
}


static void
encode(char *dest, int n)
{
    char *p;
    int i;
    int j;
    char b4[4];

    b4[0] = (dest[0] >> 2) & 0x3F;
    b4[1] = ((dest[0] & 0x03) << 4) | ((dest[1] >> 4) & 0x0F);
    b4[2] = ((dest[1] & 0x0F) << 2) | ((dest[2] >> 6) & 0x03);
    b4[3] = (char) (n == 3 ? dest[2] & 0x3F : n);

    for (p = b4, i = Count, dest = &Buffer[i], j = 4; --j >= 0; i++) {
        if (i == 13) {
            dumpcode(Buffer, 13);
            dest = Buffer;
            i = 0;
        }
        *dest++ = *p++;
    }
    Count = i;
}


int
main(void)
{
    char *p;
    int c;
    char b3[3];

    for (p = b3; (c = getchar()) != EOF;) {
        *p++ = (char) c;
        if (p == &b3[3]) {
            encode(b3, 3);
            p = b3;
        }
    }
    encode(b3, (int) (p - b3));
    flushout();
    exit(0);
    /* NOTREACHED */
}
