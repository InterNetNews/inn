/* -*- c -*-
 *
 * Author:      Christophe Wolfhugel <wolf@pasteur.fr>
 *		(although he wouldn't recognise it anymore so don't blame him)
 * File:        perl.c
 * RCSId:       $Id$
 * Description: Perl hooks for nnrpd.
 * 
 */

#if ! defined (lint)
static const char *rcsid = "$Id$" ;
static void use_rcsid (const char *rid) {   /* Never called */
  use_rcsid (rcsid) ; use_rcsid (rid) ;
}
#endif


#include <stdio.h>
#include <sys/types.h>
#include "configdata.h"
#include "clibrary.h"
#include "paths.h"
#include "post.h"
#include "logging.h"

#if defined(DO_PERL)

#include <EXTERN.h>
#include <perl.h>

extern BOOL PerlFilterActive;
extern HEADER	Table[], *EndOfTable;
extern char LogName[];

char *
HandleHeaders()
{
   dSP;
   HEADER	*hp;
   HV		*hdr;
   int		rc;
   char		*p;
   static char	buf[256];

   if (!PerlFilterActive)
       return NULL; /* not really necessary */

   ENTER ;
   SAVETMPS ;
   
   /* Create the Perl Hash */
   hdr = perl_get_hv("hdr", TRUE);
   for (hp = Table; hp < EndOfTable; hp++) {
      if (hp->Value)
         hv_store(hdr, (char *) hp->Name, strlen(hp->Name), newSVpv(hp->Value, 0), 0);
   }

   rc = perl_call_argv("filter_post", G_EVAL|G_SCALAR, NULL);

   SPAGAIN;

   hv_undef (hdr);

   buf [0] = '\0' ;
   
   if (SvTRUE(GvSV(errgv)))     /* check $@ */ {
       syslog (L_ERROR,"Perl function filter_post died: %s",
               SvPV(GvSV(errgv), na)) ;
       POPs ;
       PerlFilter (FALSE) ;
   } else if (rc == 1) {
       p = POPp;
       if (p != NULL && *p != '\0') {
           strncpy(buf, p, sizeof(buf) - 1);
           buf[sizeof(buf) - 1] = '\0';
       }
   }

   FREETMPS ;
   LEAVE ;
   
   if (buf[0] != '\0') 
      return buf ;
   return NULL;
}

#endif /* defined(DO_PERL) */

