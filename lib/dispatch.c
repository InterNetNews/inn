/*  $Id$
**
**  Dispatch cvectors of commands to functions.
**
**  This is a generic command dispatching system designed primary to handle
**  dispatching NNTP commands to functions that handle them, for NNTP server
**  software.
*/

#include "config.h"
#include "clibrary.h"

#include "inn/dispatch.h"
#include "inn/vector.h"


/*
**  Comparison function for bsearch on the table of dispatch instructions.
*/
static int
compare_dispatch(const void *key, const void *element)
{
    const char *command = key;
    const struct dispatch *rule = element;

    return strcasecmp(command, rule->command);
}


/*
**  The dispatch function.  Takes a command (as a struct cvector), the
**  dispatch table (a SORTED array of dispatch structs), the number of
**  elements in the table, the callback function for unknown commands, the
**  callback function for syntax errors (commands called with the wrong number
**  of arguments), and an opaque void * that will be passed to the callback
**  functions.
*/
void
dispatch(struct cvector *command, const struct dispatch *table, size_t count,
         dispatch_func unknown, dispatch_func syntax, void *cookie)
{
    struct dispatch *rule;
    int argc = command->count - 1;

    if (argc < 0) {
        (*unknown)(command, cookie);
        return;
    }
    rule = bsearch(command->strings[0], table, count, sizeof(struct dispatch),
                   compare_dispatch);
    if (rule == NULL)
        (*unknown)(command, cookie);
    else if (argc < rule->min_args || argc > rule->max_args)
        (*syntax)(command, cookie);
    else
        (*rule->callback)(command, cookie);
}
