#include <sys/types.h>
#include <netdb.h>
#include "configdata.h"
#include "clibrary.h"

static const char * const pvt_h_errlist[] = {
  "Resolver Error 0 (no error)",
  "Unknown host",                         /* 1 HOST_NOT_FOUND */
  "Host name lookup failure",             /* 2 TRY_AGAIN */
  "Unknown server error",                 /* 3 NO_RECOVERY */
  "No address associated with name",      /* 4 NO_ADDRESS */
};

static int pvt_h_nerr = (sizeof pvt_h_errlist / sizeof pvt_h_errlist[0]);

#if defined(hpux) || defined(__hpux) || defined(_SCO_DS)
extern int h_errno;
#endif

/* return a friendly string for the current value of h_errno. Pinched from
   Stevens */
const char *hstrerror(int err)
{
  static char buf[SMBUF];
  if (err != 0)
    {
      if (err > 0 && err < pvt_h_nerr)
        return(pvt_h_errlist[err]) ;
      else {
        sprintf (buf,"(herrno = %d)", h_errno) ;
	return(buf);
      }
    }
  else
    return("Resolver Error 0 (no error)");
}
