/*  $Revision$
**
**  Encapsulate dirent/direct differences.
*/

#if defined(HAVE_DIRENT_H)
#include <dirent.h>
typedef struct dirent	DIRENTRY;
#else 
#include <sys/dir.h>
typedef struct direct	DIRENTRY;
#endif
