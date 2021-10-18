/*
 * Replacement for a missing inet_aton.
 *
 * Provides the same functionality as the standard library routine
 * inet_aton for those platforms that don't have it.  inet_aton is
 * thread-safe.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2000-2001, 2017, 2019-2020 Russ Allbery <eagle@eyrie.org>
 * Copyright 2008, 2011, 2014
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * Copying and distribution of this file, with or without modification, are
 * permitted in any medium without royalty provided the copyright notice and
 * this notice are preserved.  This file is offered as-is, without any
 * warranty.
 *
 * SPDX-License-Identifier: FSFAP
 */

#include "config.h"
#include "portable/socket.h"
#include "portable/system.h"

/*
 * If we're running the test suite, rename inet_aton to avoid conflicts with
 * the system version.
 */
#if TESTING
#    undef inet_aton
#    define inet_aton test_inet_aton
int test_inet_aton(const char *, struct in_addr *);
#endif

int
inet_aton(const char *s, struct in_addr *addr)
{
    unsigned octet[4];
    uint32_t address;
    const char *p;
    unsigned int base, i;
    unsigned int part = 0;

    if (s == NULL)
        return 0;

    /*
     * Step through each period-separated part of the address.  If we see
     * more than four parts, the address is invalid.
     */
    for (p = s; *p != 0; part++) {
        if (part > 3)
            return 0;

        /*
         * Determine the base of the section we're looking at.  Numbers are
         * represented the same as in C; octal starts with 0, hex starts
         * with 0x, and anything else is decimal.
         */
        if (*p == '0') {
            p++;
            if (*p == 'x') {
                p++;
                base = 16;
            } else {
                base = 8;
            }
        } else {
            base = 10;
        }

        /*
         * Make sure there's actually a number.  (A section of just "0"
         * would set base to 8 and leave us pointing at a period; allow
         * that.)
         */
        if (*p == '.' && base != 8)
            return 0;
        octet[part] = 0;

        /*
         * Now, parse this segment of the address.  For each digit, multiply
         * the result so far by the base and then add the value of the digit.
         * Be careful of arithmetic overflow in cases where an unsigned long
         * is 32 bits; we need to detect it *before* we multiply by the base
         * since otherwise we could overflow and wrap and then not detect the
         * error.
         */
        for (; *p != 0 && *p != '.'; p++) {
            if (octet[part] > 0xffffffffUL / base)
                return 0;

            /*
             * Use a switch statement to parse each digit rather than assuming
             * ASCII.  Probably pointless portability.
             */
            /* clang-format off */
            switch (*p) {
            case '0':           i = 0;  break;
            case '1':           i = 1;  break;
            case '2':           i = 2;  break;
            case '3':           i = 3;  break;
            case '4':           i = 4;  break;
            case '5':           i = 5;  break;
            case '6':           i = 6;  break;
            case '7':           i = 7;  break;
            case '8':           i = 8;  break;
            case '9':           i = 9;  break;
            case 'A': case 'a': i = 10; break;
            case 'B': case 'b': i = 11; break;
            case 'C': case 'c': i = 12; break;
            case 'D': case 'd': i = 13; break;
            case 'E': case 'e': i = 14; break;
            case 'F': case 'f': i = 15; break;
            default:            return 0;
            }
            /* clang-format on */
            if (i >= base)
                return 0;
            octet[part] = (octet[part] * base) + i;
        }

        /*
         * Advance over periods; the top of the loop will increment the count
         * of parts we've seen.  We need a check here to detect an illegal
         * trailing period.
         */
        if (*p == '.') {
            p++;
            if (*p == 0)
                return 0;
        }
    }
    if (part == 0)
        return 0;

    /* IPv4 allows three types of address specification:
     *
     *     a.b
     *     a.b.c
     *     a.b.c.d
     *
     * If there are fewer than four segments, the final segment accounts for
     * all of the remaining portion of the address.  For example, in the a.b
     * form, b is the final 24 bits of the address.  We also allow a simple
     * number, which is interpreted as the 32-bit number corresponding to the
     * full IPv4 address.
     *
     * The first for loop below ensures that any initial segments represent
     * only 8 bits of the address and builds the upper portion of the IPv4
     * address.  Then, the remaining segment is checked to make sure it's no
     * bigger than the remaining space in the address and then is added into
     * the result.
     */
    address = 0;
    for (i = 0; i < part - 1; i++) {
        if (octet[i] > 0xff)
            return 0;
        address |= octet[i] << (8 * (3 - i));
    }
    if (octet[i] > (0xffffffffUL >> (i * 8)))
        return 0;
    address |= octet[i];
    if (addr != NULL)
        addr->s_addr = htonl(address);
    return 1;
}
