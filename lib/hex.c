/*
**  Convert data to or from an ASCII hex representation.
**
**  Converts arbitrary binary data to or from a representation as an ASCII
**  string of hex digits.  Used primarily for converting MD5 hashes into a
**  human-readable value.  For backward-compatibility reasons, capital letters
**  are used for hex digits > 9.
**
**  Rewritten by Russ Allbery in 2005.
**
**  Various bug fixes, code and documentation improvements since then
**  in 2021, 2024, 2026.
*/

#include "portable/system.h"

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
    size_t input, output;

    if (buflen == 0)
        return;
    for (input = 0, output = 0; input < length && output < buflen - 1;
         input++) {
        buffer[output++] = hex[(data[input] & 0xf0) >> 4];
        if (output < buflen - 1)
            buffer[output++] = hex[data[input] & 0x0f];
    }
    buffer[output] = '\0';
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
    size_t i;
    unsigned char part;

    if (buflen == 0)
        return;
    memset(buffer, 0, buflen);
    for (i = 0; (i / 2) < buflen; i++) {
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
