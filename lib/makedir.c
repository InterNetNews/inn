#include "config.h"
#include "clibrary.h"
#include <errno.h>
#include <sys/stat.h>

#include "libinn.h"

/*
**  Try to make one directory.  Return FALSE on error.
*/
static bool MakeDir(char *Name)
{
    struct stat		Sb;

    if (mkdir(Name, GROUPDIR_MODE) >= 0) {
	return TRUE;
    }

    /* See if it failed because it already exists. */
    if (stat(Name, &Sb) >= 0 && S_ISDIR(Sb.st_mode)) {
	errno = 0;
	return TRUE;
    }
    return FALSE;
}


/*
**  Given a directory, comp/foo/bar, create that directory and all
**  intermediate directories needed.  Return TRUE if ok, else FALSE.
*/
bool MakeDirectory(char *Name, bool Recurse)
{
    char	        *p;
    bool		made;

    /* Optimize common case -- parent almost always exists. */
    if (MakeDir(Name))
	return TRUE;

    if (!Recurse)
	return FALSE;

    /* Try to make each of comp and comp/foo in turn. */
    for (p = (Name[0] == '/') ? &Name[1] : Name; *p; p++)
	if (*p == '/') {
	    *p = '\0';
	    made = MakeDir(Name);
	    *p = '/';
	    if (!made)
		return FALSE;
	}

    return MakeDir(Name);
}
