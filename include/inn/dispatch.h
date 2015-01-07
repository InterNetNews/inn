/*  $Id$
**
**  Dispatch cvectors of commands to functions.
**
**  This is a generic command dispatching system designed primary to handle
**  dispatching NNTP commands to functions that handle them, for NNTP server
**  software.
*/

#ifndef INN_DISPATCH_H
#define INN_DISPATCH_H 1

#include <inn/defines.h>

/* Forward declarations. */
struct cvector;

BEGIN_DECLS

/* The type of a callback function for a command.  All callback functions take
   a struct cvector holding the command and a void * pointer containing
   arbitrary data that was provided by the calling program to the dispatch
   function. */
typedef void (*dispatch_func)(struct cvector *, void *);

/* Dispatch instructions for a command.  The dispatch function takes an array
   of these structs, each of which defines a single command.  A command
   description contains the case-insensitive command, which should be the
   first string of the cvector, the callback function for that command, the
   minimum and maximum number of arguments allowed, and a description string
   that isn't used by the dispatch function. */
struct dispatch {
    const char *command;
    dispatch_func callback;
    int min_args;
    int max_args;
    const char *description;
};

/* The dispatch function.  Takes a command (as a struct cvector), the dispatch
   table (a SORTED array of dispatch structs), the number of elements in the
   table, the callback function for unknown commands, the callback function
   for syntax errors (commands called with the wrong number of arguments), and
   an opaque void * that will be passed to the callback functions.

   Note that the dispatch table *must* be sorted by the name of the command
   (case-insensitive, strcasecmp(3) order. */
void dispatch(struct cvector *command, const struct dispatch *, size_t count,
              dispatch_func unknown, dispatch_func syntax, void *);

END_DECLS

#endif /* INN_DISPATCH_H */
