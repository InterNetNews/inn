/*  $Id$
**
**  Replacement for a missing setproctitle.
**
**  Provides the same functionality as the BSD function setproctitle on hosts
**  where modifying argv will produce those results, or on HP-UX (which has
**  its own peculiar way of doing this).  This may be ineffective on some
**  platforms.
**
**  Before calling setproctitle, it is *required* that setproctitle_init be
**  called, passing it argc and argv as arguments.  setproctitle_init will be
**  stubbed out on those platforms that don't need it.
*/

#include "config.h"
#include "clibrary.h"
#include "portable/setproctitle.h"

#include "inn/messages.h"

#if HAVE_PSTAT

#include <sys/param.h>
#include <sys/pstat.h>

void
setproctitle(const char *format, ...)
{
    va_list args;
    char title[BUFSIZ];
    union pstun un;
    ssize_t delta = 0;

    if (message_program_name != NULL) {
        delta = snprintf(title, sizeof(title), "%s: ", message_program_name);
        if (delta < 0)
            delta = 0;
    }
    va_start(args, format);
    vsnprintf(title + delta, sizeof(title) - delta, format, args);
    va_end(args);
    un.pst_command = title;
    pstat(PSTAT_SETCMD, un, strlen(title), 0, 0);
}

#else

static char *title_start = NULL;
static char *title_end = NULL;

void
setproctitle_init(int argc, char *argv[])
{
    title_start = argv[0];
    title_end = argv[argc - 1] + strlen(argv[argc - 1]) - 1;
}

void
setproctitle(const char *format, ...)
{
    va_list args;
    size_t length;
    ssize_t delta;
    char *title;

    if (title_start == NULL || title_end == NULL) {
        warn("setproctitle called without setproctitle_init");
        return;
    }

    /* setproctitle prepends the program name to its arguments.  Our emulation
       should therefore do the same thing.  However, some operating systems
       seem to do that automatically even when we completely overwrite argv,
       so start our title with a - so that they'll instead put (nnrpd) at the
       end, thinking we're swapped out. */
    title = title_start;
    *title++ = '-';
    *title++ = ' ';
    length = title_end - title_start - 2;

    /* Now, put in the actual content.  Get the program name from
       message_program_name if it's set. */
    if (message_program_name != NULL) {
        delta = snprintf(title, length, "%s: ", message_program_name);
        if (delta < 0 || (size_t) delta > length)
            return;
        if (delta > 0) {
            title += delta;
            length -= delta;
        }
    }
    va_start(args, format);
    delta = vsnprintf(title, length, format, args);
    va_end(args);
    if (delta < 0 || (size_t) delta > length)
        return;
    if (delta > 0) {
        title += delta;
        length -= delta;
    }
    for (; length > 1; length--, title++)
        *title = ' ';
    *title = '\0';
}

#endif /* !HAVE_PSTAT */
