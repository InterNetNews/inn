/*  $Id$
**
**  Wire format article utilities.
**
**  Originally written by Alex Kiernan (alex.kiernan@thus.net)
**
**  These routines manipulate wire format articles; in particular, they should
**  be safe in the presence of embedded NULs and UTF-8 characters.
*/

#ifndef INN_WIRE_H
#define INN_WIRE_H 1

#include <inn/defines.h>
#include <sys/types.h>          /* size_t */

BEGIN_DECLS

/* Given a pointer to the start of an article, locate the first octet
   of the body (which may be the octet beyond the end of the buffer if
   your article is bodyless). */
char *wire_findbody(const char *, size_t);

/* Given a pointer into an article and a pointer to the end of the article,
   find the start of the next line or return NULL if there are no more lines
   remaining in the article. */
char *wire_nextline(const char *, const char *end);

/* Given a pointer to the start of an article and the name of a header, find
   the beginning of the value of the given header (the returned pointer will
   be after the name of the header and any initial whitespace).  Headers whose
   only content is whitespace are ignored.  If the header isn't found, returns
   NULL.

   WARNING: This function does not comply with RFC 2822's idea of header
   content, particularly in its skipping of initial whitespace. */
char *wire_findheader(const char *article, size_t, const char *header);

/* Given a pointer inside a header's value and a pointer to the end of the
   article, returns a pointer to the end of the header value (the \n at the
   end of the terminating \r\n with folding taken into account), or NULL if no
   such terminator was found before the end of the article. */
char *wire_endheader(const char *header, const char *end);

END_DECLS

#endif /* INN_WIRE_H */
