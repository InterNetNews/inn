#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "configdata.h"
#include "clibrary.h"
#include "libinn.h"

/*
**  Try to make one directory.  Return FALSE on error.
*/
STATIC BOOL MakeDir(char *Name)
{
    struct stat		Sb;

    if (mkdir(Name, GROUPDIR_MODE) >= 0) {
	return TRUE;
    }

    /* See if it failed because it already exists. */
    return stat(Name, &Sb) >= 0 && S_ISDIR(Sb.st_mode);
}


/*
**  Given a directory, comp/foo/bar, create that directory and all
**  intermediate directories needed.  Return TRUE if ok, else FALSE.
*/
BOOL MakeDirectory(char *Name, BOOL Recurse)
{
    char	        *p;
    BOOL		made;

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
