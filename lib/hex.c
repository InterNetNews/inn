/*  $Id$
**
**  Convert data to or from an ASCII hex representation.
**
**  Converts arbitrary binary data to or from a representation as an ASCII
**  string of hex digits.  Used primarily for converting MD5 hashes into a
**  human-readable value.  For backward-compatibility reasons, capital letters
**  are used for hex digits > 9.
*/

#include "config.h"
#include "clibrary.h"
#include "inn/utility.h"

/*
**  Convert data to an ASCII hex representation.  The result will be stored in
**  buffer and nul-terminated.  buflen is the length of the buffer, which must
**  be at least 2 * length + 1 to hold the full representation.  If it is not
**  long enough, the result will be truncated but buffer will still be
**  nul-terminated.
*/
void
inn_encode_hex(const unsigned char *data, size_t length, char *buffer,
               size_t buflen)
{
    static const char hex[] = "0123456789ABCDEF";
    const unsigned char *p;
    unsigned int i;

    if (buflen == 0)
        return;
    for (p = data, i = 0; i < length && (i * 2) < (buflen - 1); p++, i++) {
        buffer[i * 2]       = hex[(*p & 0xf0) >> 4];
        buffer[(i * 2) + 1] = hex[(*p & 0x0f)];
    }
    if (length * 2 > buflen - 1)
        buffer[buflen - 1] = '\0';
    else
        buffer[length * 2] = '\0';
}


/*
**  Convert data from an ASCII hex representation.  No adjustment is made for
**  byte order.  The conversion stops at the first character that is not a
**  valid hex character.  If there are an uneven number of valid input
**  characters, the input is zero-padded at the *end* (so the string "F" is
**  equivalent to the string "F0").  Lowercase hex digits are tolerated, even
**  though inn_encode_hex doesn't produce them.  buflen is the length of the
**  output buffer and must be at least (input - 1) / 2.  If it is too short
**  to hold the full data, the result will be truncated.
*/
void
inn_decode_hex(const char *data, unsigned char *buffer, size_t buflen)
{
    const char *p;
    unsigned int i;
    unsigned char part;

    if (buflen == 0)
        return;
    memset(buffer, 0, buflen);
    for (p = data, i = 0; (i / 2) < buflen; p++, i++) {
        if (data[i] >= '0' && data[i] <= '9')
            part = data[i] - '0';
        else if (data[i] >= 'A' && data[i] <= 'F')
            part = data[i] - 'A' + 10;
        else if (data[i] >= 'a' && data[i] <= 'f')
            part = data[i] - 'a' + 10;
        else
            return;
        if (i % 2 == 0)
            part <<= 4;
        buffer[i / 2] |= part;
    }
}
