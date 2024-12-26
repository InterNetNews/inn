/*
**  Decode seven-bit input into full binary output.
**  From @(#)decode.c 1.3 5/15/85, distributed with B2.11 News.
**
**  Various bug fixes, code and documentation improvements since then
**  in 1997, 1999, 2000, 2002, 2003, 2021, 2024.
**
**  Collect runs of 12 seven-bit characters.  Combine them in pairs to
**  make six 13-bit characters.  Extract the top bit of each pair to make
**  a 13th six-bit character, and split the remaining six 12-bit
**  characters to form 12 six-bit characters.  Collect four six-bit
**  characters and convert it to three eight-bit characters.
**
**  Got that?  All the remaining work in this program is to get the
**  ending conditions right.
*/

#include "portable/system.h"

#include "inn/messages.h"


/*
**  These characters can't appear in normal output, so we use them to
**  mark that the data that follows is the terminator.  The character
**  immediately following this pair is the length of the terminator (which
**  otherwise might be indeterminable)
*/
#define ENDMARK1 ((90 * 91 + 90) / 91)
#define ENDMARK2 ((90 * 91 + 90) % 91)


static char Buffer[4];
static int count;


static void
pack6(int n, int last)
{
    char *q;
    int i;
    char b3[3];

    i = 3;
    if (last && (i = Buffer[n - 1]) >= 3) {
        /* Do the best we can. */
        warn("badly-terminated file");
        i = 3;
    }

    b3[0] = (Buffer[0] << 2) | ((Buffer[1] >> 4) & 0x03);
    b3[1] = (Buffer[1] << 4) | ((Buffer[2] >> 2) & 0x0F);
    b3[2] = (Buffer[2] << 6) | (Buffer[3] & 0x3F);
    for (q = b3; --i >= 0;)
        putchar(*q++);
}


static void
pack12(char *p, int n, int last)
{
    char *q;
    int c13;
    int c;
    int i;
    char b13[13];
    char b3[3];

    for (q = b13, c13 = 0, i = 0; i < n; i += 2) {
        c = *p++ * 91;
        c += *p++;
        c13 <<= 1;
        if (c & (1 << 12))
            c13 |= 1;
        *q++ = (c >> 6) & 0x3F;
        *q++ = c & 0x3F;
    }
    *q++ = (char) c13;
    if (last)
        q = &b13[last];

    for (p = b13, n = q - p, i = count, q = &Buffer[count]; --n > 0;) {
        *q++ = *p++;
        if (++i == 4) {
            /* Inline expansion of pack6. */
            b3[0] = (Buffer[0] << 2) | ((Buffer[1] >> 4) & 0x03);
            b3[1] = (Buffer[1] << 4) | ((Buffer[2] >> 2) & 0x0F);
            b3[2] = (Buffer[2] << 6) | (Buffer[3] & 0x3F);
            putchar(b3[0]);
            putchar(b3[1]);
            putchar(b3[2]);
            i = 0;
            q = Buffer;
        }
    }

    /* The last octet. */
    *q++ = *p++;
    i++;

    if (last || i == 4) {
        pack6(i, last);
        i = 0;
    }

    count = i;
}


int
main(void)
{
    int c;
    char *p;
    int i;
    int first;
    int cnt;
    char *base;
    char b12[12];
    char c12[12];

    message_program_name = "decode";

    base = p = b12;
    for (i = 12, cnt = 0, first = 1; (c = getchar()) != EOF;) {
        if (c < ' ' || c >= ' ' + 91)
            die("bad data");
        if (i == 10 && p[-1] == ENDMARK1 && p[-2] == ENDMARK2) {
            cnt = c - ' ';
            i = 12;
            p -= 2;
            continue;
        }
        *p++ = c - ' ';
        if (--i == 0) {
            if (p == &b12[12]) {
                if (!first)
                    pack12(c12, 12, 0);
                else
                    first = 0;
                base = p = c12;
            } else {
                pack12(b12, 12, 0);
                base = p = b12;
            }
            i = 12;
        }
    }

    if (base == b12) {
        if (!first)
            pack12(c12, 12, i == 12 ? cnt : 0);
    } else
        pack12(b12, 12, i == 12 ? cnt : 0);

    if (i != 12)
        pack12(base, 12 - i, cnt);

    exit(0);
    /* NOTREACHED */
}
