#ifndef SQLITE_HELPER_H
#define SQLITE_HELPER_H 1

#include "config.h"

#ifdef HAVE_SQLITE3

#    include <sqlite3.h>
#    include <stddef.h>

/* SQLITE_PREPARE_PERSISTENT is defined in SQLite 3.20.0 and above. */
#    ifndef SQLITE_PREPARE_PERSISTENT
#        define SQLITE_PREPARE_PERSISTENT 0x00
#    endif

typedef struct sqlite_helper_t {
    size_t stmt_count;
    char const *text;
} sqlite_helper_t;

BEGIN_DECLS

/*
 * Execute initialisation statements and prepare named statements.
 *
 * Returns a SQLite error code.
 *
 * In case of error, the caller is responsible for deallocating
 * the returned error message with sqlite3_free.
 */

extern int sqlite_helper_init(sqlite_helper_t const *helper,
                              sqlite3_stmt **stmts, sqlite3 *connection,
                              unsigned int prepare_flags, char **errmsg);

/*
 * Deallocate all the prepared statements.
 */

extern void sqlite_helper_term(sqlite_helper_t const *helper,
                               sqlite3_stmt **stmts);

END_DECLS

#endif /* HAVE_SQLITE3 */

#endif /* ! SQLITE_HELPER_H */
