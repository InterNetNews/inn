#include "sqlite-helper.h"

#ifdef HAVE_SQLITE3

#    include <stdio.h>
#    include <string.h>

/* sqlite3_prepare_v3() is defined in SQLite 3.20.0 and above. */
#    if SQLITE_VERSION_NUMBER >= 3020000
#        define UNUSED_BEFORE_SQLITE_3_20
#    else
#        define UNUSED_BEFORE_SQLITE_3_20 UNUSED
#        define sqlite3_prepare_v3(db, zSql, nByte, prepFlags, ppStmt, \
                                   pzTail)                             \
            sqlite3_prepare_v2(db, zSql, nByte, ppStmt, pzTail)
#    endif

int
sqlite_helper_init(sqlite_helper_t const *helper, sqlite3_stmt **stmts,
                   sqlite3 *connection,
                   unsigned int prepare_flags UNUSED_BEFORE_SQLITE_3_20,
                   char **errmsg)
{
    int result;
    size_t ix, count;
    char const *text, *textend;
    char const *name, *nameend;

    text = helper->text;
    result = sqlite3_exec(connection, text, 0, NULL, errmsg);
    if (result != SQLITE_OK)
        return result;
    name = strchr(text, 0) + 1;
    count = helper->stmt_count;
    for (ix = 0; ix < count; ix++) {
        nameend = strchr(name, 0);
        text = nameend + 1;
        textend = strchr(text, 0);
        result = sqlite3_prepare_v3(connection, text, textend - text,
                                    prepare_flags, stmts + ix, NULL);
        if (result != SQLITE_OK)
            goto oops;
        name = textend + 1;
    }
    return SQLITE_OK;

oops:
    if (errmsg) {
        size_t namelen;
        char const *err;
        char *errcopy;

        namelen = nameend - name;
        err = sqlite3_errmsg(connection);
        if (err) {
            size_t errlen;

            errlen = strlen(err);
            errcopy = sqlite3_malloc(namelen + errlen + 3);
            if (errcopy) {
                memcpy(errcopy, name, namelen);
                errcopy[namelen] = ':';
                errcopy[namelen + 1] = ' ';
                memcpy(errcopy + namelen + 2, err, errlen + 1);
            }
        } else {
            errcopy = sqlite3_malloc(namelen + 1);
            if (errcopy)
                memcpy(errcopy, name, namelen + 1);
        }
        *errmsg = errcopy;
    }
    while (ix-- > 0) {
        sqlite3_finalize(stmts[ix]);
        stmts[ix] = NULL;
    }
    return result;
}

void
sqlite_helper_term(sqlite_helper_t const *helper, sqlite3_stmt **stmts)
{
    size_t ix, count;

    count = helper->stmt_count;
    for (ix = 0; ix < count; ix++) {
        sqlite3_finalize(stmts[ix]);
        stmts[ix] = NULL;
    }
}

#endif /* HAVE_SQLITE3 */
