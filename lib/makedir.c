#include "config.h"
#include "clibrary.h"
#include <errno.h>
#include <sys/stat.h>

#include "inn/libinn.h"

/*
**  Try to make one directory.  Return false on error.
*/
static bool MakeDir(char *Name)
{
    struct stat		Sb;

    if (mkdir(Name, GROUPDIR_MODE) >= 0) {
	return true;
    }

    /* See if it failed because it already exists. */
    if (stat(Name, &Sb) >= 0 && S_ISDIR(Sb.st_mode)) {
	errno = 0;
	return true;
    }
    return false;
}


/*
**  Given a directory, comp/foo/bar, create that directory and all
**  intermediate directories needed.  Return true if ok, else false.
*/
bool MakeDirectory(char *Name, bool Recurse)
{
    char	        *p;
    bool		made;

    /* Optimize common case -- parent almost always exists. */
    if (MakeDir(Name))
	return true;

    if (!Recurse)
	return false;

    /* Try to make each of comp and comp/foo in turn. */
    for (p = (Name[0] == '/') ? &Name[1] : Name; *p; p++)
	if (*p == '/') {
	    *p = '\0';
	    made = MakeDir(Name);
	    *p = '/';
	    if (!made)
		return false;
	}

    return MakeDir(Name);
}
