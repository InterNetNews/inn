/*  $Id$
**
**  wildmat pattern matching with Unicode UTF-8 extensions.
**
**  Do shell-style pattern matching for ?, \, [], and * characters.  Might not
**  be robust in face of malformed patterns; e.g., "foo[a-" could cause a
**  segmentation violation.  It is 8-bit clean.  (Robustness hopefully fixed
**  July 2000; all malformed patterns should now just fail to match anything.)
**
**  Original by Rich $alz, mirror!rs, Wed Nov 26 19:03:17 EST 1986.
**  Rich $alz is now <rsalz@osf.org>.
**
**  April, 1991:  Replaced mutually-recursive calls with in-line code for the
**  star character.
**
**  Special thanks to Lars Mathiesen <thorinn@diku.dk> for the ABORT code.
**  This can greatly speed up failing wildcard patterns.  For example:
**
**	pattern: -*-*-*-*-*-*-12-*-*-*-m-*-*-*
**	text 1:	 -adobe-courier-bold-o-normal--12-120-75-75-m-70-iso8859-1
**	text 2:	 -adobe-courier-bold-o-normal--12-120-75-75-X-70-iso8859-1
**
**  Text 1 matches with 51 calls, while text 2 fails with 54 calls.  Without
**  the ABORT code, it takes 22310 calls to fail.  Ugh.  The following
**  explanation is from Lars:
**
**  The precondition that must be fulfilled is that DoMatch will consume at
**  least one character in text.  This is true if *p is neither '*' nor '\0'.)
**  The last return has ABORT instead of false to avoid quadratic behaviour in
**  cases like pattern "*a*b*c*d" with text "abcxxxxx".  With false, each
**  star-loop has to run to the end of the text; with ABORT only the last one
**  does.
**
**  Once the control of one instance of DoMatch enters the star-loop, that
**  instance will return either true or ABORT, and any calling instance will
**  therefore return immediately after (without calling recursively again).
**  In effect, only one star-loop is ever active.  It would be possible to
**  modify the code to maintain this context explicitly, eliminating all
**  recursive calls at the cost of some complication and loss of clarity (and
**  the ABORT stuff seems to be unclear enough by itself).  I think it would
**  be unwise to try to get this into a released version unless you have a
**  good test data base to try it out on.
**
**  June, 1991:  Robert Elz <kre@munnari.oz.au> added minus and close bracket
**  handling for character sets.
**
**  July, 2000:  Largely rewritten by Russ Allbery <rra@stanford.edu> to add
**  support for ',', '!', and optionally '@' to the core wildmat routine.
**  Broke the character class matching into a separate function for clarity
**  since it's infrequently used in practice, and added some simple lookahead
**  to significantly decrease the recursive calls in the '*' matching code.
**  Added support for UTF-8 as the default character set for any high-bit
**  characters.
**
**  For more information on UTF-8, see RFC 3629.
**
**  Please note that this file is intentionally written so that conditionally
**  executed expressions are on separate lines from the condition to
**  facilitate analysis of the coverage of the test suite using purecov.
**  Please preserve this.  As of March 11, 2001, purecov reports that the
**  accompanying test suite achieves 100% coverage of this file.
*/

#include "config.h"
#include "clibrary.h"
#include "inn/libinn.h"

#define ABORT -1

/* Whether or not an octet looks like the start of a UTF-8 character. */
#define ISUTF8(c)       (((c) & 0xc0) == 0xc0)


/*
**  Determine the length of a non-ASCII character in octets (for advancing
**  pointers when skipping over characters).  Takes a pointer to the start of
**  the character and to the last octet of the string.  If end is NULL, expect
**  the string pointed to by start to be nul-terminated.  If the character is
**  malformed UTF-8, return 1 to treat it like an eight-bit local character.
*/
static int
utf8_length(const unsigned char *start, const unsigned char *end)
{
    unsigned char mask = 0x80;
    const unsigned char *p;
    int length = 0;
    int left;

    for (; mask > 0 && (*start & mask) == mask; mask >>= 1)
        length++;
    if (length < 2 || length > 6)
        return 1;
    if (end != NULL && (end - start + 1) < length)
        return 1;
    left = length - 1;
    for (p = start + 1; left > 0 && (*p & 0xc0) == 0x80; p++)
        left--;
    return (left == 0) ? length : 1;
}


/*
**  Check whether a string contains only valid UTF-8 characters.
*/
bool
is_valid_utf8(const char *text)
{
    unsigned char mask;
    const unsigned char *p;
    int length;
    int left;

    for (p = (const unsigned char *)text; *p != '\0';) {
        mask = 0x80;
        length = 0;

        /* Find out the expected length of the character. */
        for (; mask > 0 && (*p & mask) == mask; mask >>= 1)
            length++;

        p++;

        /* Valid ASCII. */
        if (length == 0)
            continue;
        
        /* Invalid length. */
        if (length < 2 || length > 6)
            return false;

        /* Check that each byte looks like 10xxxxxx, except for the first. */
        left = length - 1;
        for (; left > 0 && (*p & 0xc0) == 0x80; p++)
            left--;

        if (left > 0)
            return false;
    }

    return true;
}


/*
**  Convert a UTF-8 character to UCS-4.  Takes a pointer to the start of the
**  character and to the last octet of the string, and to a uint32_t into
**  which to put the decoded UCS-4 value.  If end is NULL, expect the string
**  pointed to by start to be nul-terminated.  Returns the number of octets in
**  the UTF-8 encoding.  If the UTF-8 character is malformed, set result to
**  the decimal value of the first octet; this is wrong, but it will generally
**  cause the rest of the wildmat matching to do the right thing for non-UTF-8
**  input.
*/
static int
utf8_decode(const unsigned char *start, const unsigned char *end,
            uint32_t *result)
{
    uint32_t value = 0;
    int length, i;
    const unsigned char *p = start;
    unsigned char mask;

    length = utf8_length(start, end);
    if (length < 2) {
        *result = *start;
        return 1;
    }
    mask = (1 << (7 - length)) - 1;
    value = *p & mask;
    p++;
    for (i = length - 1; i > 0; i--) {
        value = (value << 6) | (*p & 0x3f);
        p++;
    }
    *result = value;
    return length;
}


/*
**  Match a character class against text, a UCS-4 character.  start is a
**  pointer to the first character of the character class, end a pointer to
**  the last.  Returns whether the class matches that character.
*/
static bool
match_class(uint32_t text, const unsigned char *start,
            const unsigned char *end)
{
    bool reversed, allowrange;
    const unsigned char *p = start;
    uint32_t first = 0;
    uint32_t last;

    /* Check for an inverted character class (starting with ^).  If the
       character matches the character class, we return !reversed; that way,
       we return true if it's a regular character class and false if it's a
       reversed one.  If the character doesn't match, we return reversed. */
    reversed = (*p == '^');
    if (reversed)
        p++;

    /* Walk through the character class until we reach the end or find a
       match, handling character ranges as we go.  Only permit a range to
       start when allowrange is true; this allows - to be treated like a
       normal character as the first character of the class and catches
       malformed ranges like a-e-n.  We treat the character at the beginning
       of a range as both a regular member of the class and the beginning of
       the range; this is harmless (although it means that malformed ranges
       like m-a will match m and nothing else). */
    allowrange = false;
    while (p <= end) {
        if (allowrange && *p == '-' && p < end) {
            p++;
            p += utf8_decode(p, end, &last);
            if (text >= first && text <= last)
                return !reversed;
            allowrange = false;
        } else {
            p += utf8_decode(p, end, &first);
            if (text == first)
                return !reversed;
            allowrange = true;
        }
    }
    return reversed;
}


/*
**  Match the text against the pattern between start and end.  This is a
**  single pattern; a leading ! or @ must already be taken care of, and
**  commas must be dealt with outside of this routine.
*/
static int
match_pattern(const unsigned char *text, const unsigned char *start,
              const unsigned char *end)
{
    const unsigned char *q, *endclass;
    const unsigned char *p = start;
    bool ismeta;
    int matched, width;
    uint32_t c;

    for (; p <= end; p++) {
        if (!*text && *p != '*')
            return ABORT;

        switch (*p) {
        case '\\':
            if (!*++p)
                return ABORT;
            /* Fall through. */

        default:
            if (*text++ != *p)
                return false;
            break;

        case '?':
            text += ISUTF8(*text) ? utf8_length(text, NULL) : 1;
            break;

        case '*':
            /* Consecutive stars are equivalent to one.  Advance pattern to
               the character after the star. */
            for (++p; *p == '*'; p++)
                ;

            /* A trailing star will match anything. */
            if (p > end)
                return true;

            /* Basic algorithm: Recurse at each point where the * could
               possibly match.  If the match succeeds or aborts, return
               immediately; otherwise, try the next position.

               Optimization: If the character after the * in the pattern
               isn't a metacharacter (the common case), then the * has to
               consume characters at least up to the next occurrence of that
               character in the text.  Scan forward for those points rather
               than recursing at every possible point to save the extra
               function call overhead. */
            ismeta = (*p == '[' || *p == '?' || *p == '\\');
            while (*text) {
                width = ISUTF8(*text) ? utf8_length(text, NULL) : 1;
                if (ismeta) {
                    matched = match_pattern(text, p, end);
                    text += width;
                } else {
                    while (*text && *text != *p) {
                        text += width;
                        width = ISUTF8(*text) ? utf8_length(text, NULL) : 1;
                    }
                    if (!*text)
                        return ABORT;
                    matched = match_pattern(++text, p + 1, end);
                }
                if (matched != false)
                    return matched;
            }
            return ABORT;

        case '[':
            /* Find the end of the character class, making sure not to pick
               up a close bracket at the beginning of the class. */
            p++;
            q = p + (*p == '^') + 1;
            if (q > end)
                return ABORT;
            endclass = memchr(q, ']', (size_t) (end - q + 1));
            if (!endclass)
                return ABORT;

            /* Do the heavy lifting in another function for clarity, since
               character classes are an uncommon case. */
            text += utf8_decode(text, NULL, &c);
            if (!match_class(c, p, endclass - 1))
                return false;
            p = endclass;
            break;
        }
    }

    return (*text == '\0');
}


/*
**  Takes text and a wildmat expression; a wildmat expression is a
**  comma-separated list of wildmat patterns, optionally preceded by ! to
**  invert the sense of the expression.  Returns UWILDMAT_MATCH if that
**  expression matches the text, UWILDMAT_FAIL otherwise.  If allowpoison is
**  set, allow @ to introduce a poison expression (the same as !, but if it
**  triggers the failed match the routine returns UWILDMAT_POISON instead).
*/
static enum uwildmat
match_expression(const unsigned char *text, const unsigned char *start,
                 bool allowpoison)
{
    const unsigned char *end, *split;
    const unsigned char *p = start;
    bool reverse, escaped;
    bool match = false;
    bool poison = false;
    bool poisoned = false;

    /* Handle the empty expression separately, since otherwise end will be
       set to an invalid pointer. */
    if (!*p)
        return !*text ? UWILDMAT_MATCH : UWILDMAT_FAIL;
    end = start + strlen((const char *) start) - 1;

    /* Main match loop.  Find each comma that separates patterns, and attempt 
       to match the text with each pattern in order.  The last matching
       pattern determines whether the whole expression matches. */
    for (; p <= end + 1; p = split + 1) {
        if (allowpoison)
            poison = (*p == '@');
        reverse = (*p == '!') || poison;
        if (reverse)
            p++;

        /* Find the first unescaped comma, if any.  If there is none, split
           will be one greater than end and point at the nul at the end of
           the string. */
        for (escaped = false, split = p; split <= end; split++) {
            if (*split == '[') {
                split++;
                if (*split == ']')
                    split++;
                while (split <= end && *split != ']')
                    split++;
            }
            if (*split == ',' && !escaped)
                break;
            escaped = (*split == '\\') ? !escaped : false;
        }

        /* Optimization: If match == !reverse and poison == poisoned, this
           pattern can't change the result, so don't do any work. */
        if (match == !reverse && poison == poisoned)
            continue;
        if (match_pattern(text, p, split - 1) == true) {
            poisoned = poison;
            match = !reverse;
        }
    }
    if (poisoned)
        return UWILDMAT_POISON;
    return match ? UWILDMAT_MATCH : UWILDMAT_FAIL;
}


/*
**  User-level routine used for wildmats where @ should be treated as a
**  regular character.
*/
bool
uwildmat(const char *text, const char *pat)
{
    const unsigned char *utext = (const unsigned char *) text;
    const unsigned char *upat = (const unsigned char *) pat;

    if (upat[0] == '*' && upat[1] == '\0')
        return true;
    else
        return (match_expression(utext, upat, false) == UWILDMAT_MATCH);
}


/*
**  User-level routine used for wildmats that support poison matches.
*/
enum uwildmat
uwildmat_poison(const char *text, const char *pat)
{
    const unsigned char *utext = (const unsigned char *) text;
    const unsigned char *upat = (const unsigned char *) pat;

    if (upat[0] == '*' && upat[1] == '\0')
        return UWILDMAT_MATCH;
    else
        return match_expression(utext, upat, true);
}


/*
**  User-level routine for simple expressions (neither , nor ! are special).
*/
bool
uwildmat_simple(const char *text, const char *pat)
{
    const unsigned char *utext = (const unsigned char *) text;
    const unsigned char *upat = (const unsigned char *) pat;
    size_t length;

    if (upat[0] == '*' && upat[1] == '\0')
        return true;
    else {
        length = strlen(pat);
        return (match_pattern(utext, upat, upat + length - 1) == true);
    }
}
