/*  $Revision$
**
**  This is a plug-in replacement for setenv() libc function which is
**  native to BSD 4.3 so it may not be supported on SysV systems.
**
**  Written by Ilya Etingof <ilya@glas.net>, 1999.
**
*/

#include <stdlib.h>
#include <sys/types.h>
#include <ctype.h>
#include "configdata.h"
#include "clibrary.h"
#include "libinn.h"
#include "macros.h"

/*
** Set environment variable either overriding existing value
** or leaving the old one depending of the overwrite flag.
*/
int setenv(const char *name, const char *value, int overwrite)
{
	char	*arg;
	int	result;

	/* Check variable existance */
	if (!overwrite && getenv(name) != NULL)
	{
		/* Won't override */
		return 0;
	}

	/* Allocate memory for argument buffer */
	if ((arg = NEW(char, strlen(name)+strlen(value)+2)) == NULL)
	{
		return -1;
	}

	/* Format putenv() argument */
	strcpy(arg, name); strcat(arg, "="); strcat(arg, value);

	/*
	 * Put variable into environment. This leaves memory used by previous
	 * variable=value pair unfreed. :(
	 */
	result = putenv(arg);

	/* Return result */
	return result;
}
