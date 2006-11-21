/*  $Id$
**
**  Trashing articles method
*/
#include "config.h"
#include "clibrary.h"
#include "inn/libinn.h"
#include "methods.h"

#include "trash.h"

bool
trash_init(SMATTRIBUTE *attr)
{
    if (attr == NULL) {
	SMseterror(SMERR_INTERNAL, "attr is NULL");
	return false;
    }
    attr->selfexpire = true;
    attr->expensivestat = false;
    return true;
}

TOKEN
trash_store(const ARTHANDLE article, const STORAGECLASS class)
{
    TOKEN               token;

    if (article.token == (TOKEN *)NULL)
	memset(&token, '\0', sizeof(token));
    else {
	memcpy(&token, article.token, sizeof(token));
	memset(&token.token, '\0', STORAGE_TOKEN_LENGTH);
    }
    token.type = TOKEN_TRASH;
    token.class = class;
    return token;
}

ARTHANDLE *
trash_retrieve(const TOKEN token, const RETRTYPE amount UNUSED)
{
    if (token.type != TOKEN_TRASH) {
	SMseterror(SMERR_INTERNAL, NULL);
	return (ARTHANDLE *)NULL;
    }
    SMseterror(SMERR_NOENT, NULL);
    return (ARTHANDLE *)NULL;
}

void
trash_freearticle(ARTHANDLE *article UNUSED)
{
}

bool
trash_cancel(TOKEN token UNUSED)
{
    SMseterror(SMERR_NOENT, NULL);
    return false;
}

bool
trash_ctl(PROBETYPE type, TOKEN *token UNUSED, void *value UNUSED)
{
    switch (type) {
    case SMARTNGNUM:
    default:
	return false;
    }
}

bool
trash_flushcacheddata(FLUSHTYPE type UNUSED)
{
    return true;
}

void
trash_printfiles(FILE *file UNUSED, TOKEN token UNUSED, char **xref UNUSED,
                 int ngroups UNUSED)
{
}

ARTHANDLE *
trash_next(ARTHANDLE *article UNUSED, const RETRTYPE amount UNUSED)
{
    return NULL;
}

void
trash_shutdown(void)
{
}
