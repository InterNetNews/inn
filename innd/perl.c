/* -*- c -*-
 *
 * Author:      Christophe Wolfhugel <wolf@pasteur.fr>
 *		(although he wouldn't recognise it anymore so don't blame him)
 * File:        perl.c
 * RCSId:       $Id$
 * Description: perl support for innd.
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
#include <netinet/in.h>
#include "configdata.h"
#include "clibrary.h"
#include "innd.h"
#include "art.h"

#if defined(DO_PERL)

#if defined (DO_NEED_BOOL)
typedef enum { false = 0, true = 1 } bool;
#endif

#include <EXTERN.h>
#include <perl.h>

extern BOOL		PerlFilterActive;
extern ARTHEADER	ARTheaders[], *ARTheadersENDOF;
extern CV		*perl_filter_cv ;
extern char		*pathForPerl ;

char *
HandleArticle(artBody)
char *artBody;
{
   dSP;
   ARTHEADER	*hp;
   HV		*hdr;
   int		rc;
   char		*p;
   static char	buf[256];

   if (!PerlFilterActive || perl_filter_cv == NULL)
     return NULL;

   /* Create the Perl Hash */
   hdr = perl_get_hv("hdr", TRUE);
   for (hp = ARTheaders; hp < ARTheadersENDOF; hp++)
     {
       if (hp->Found && hp->Value && strcmp (hp->Name,"Path") != 0)
         hv_store(hdr, (char *) hp->Name, strlen(hp->Name), newSVpv(hp->Value, 0), 0);
     }

   /* store article body */
   if (artBody != NULL)
     hv_store(hdr, (char *) "__BODY__", 8, newSVpv(artBody, 0), 0) ;

   if (pathForPerl != NULL)
     {
       char *p = strchr (pathForPerl,'\n') ;

       *p = '\0' ;
       hv_store (hdr, (char *) "Path", 4, newSVpv(pathForPerl,0), 0) ;
       *p = '\n' ;
     }

   ENTER ;
   SAVETMPS ;

   rc = perl_call_argv ("filter_art", G_EVAL|G_SCALAR,NULL);

   SPAGAIN;

   hv_undef(hdr);

   buf [0] = '\0' ;
   
   if (SvTRUE(GvSV(errgv)))     /* check $@ */
     {
       syslog (L_ERROR,"Perl function filter_art died: %s",
               SvPV(GvSV(errgv), na)) ;
       POPs ;
       PerlFilter (FALSE) ;
     }
   else if (rc == 1)
     {
       p = POPp;

       if (p != NULL && *p != '\0')
         {
           strncpy(buf, p, sizeof(buf) - 1);
           buf[sizeof(buf) - 1] = '\0';
         }
     }
 
   PUTBACK;
   FREETMPS;
   LEAVE;

   if (buf[0] != '\0') 
      return buf ;
   return NULL;
}


char *
HandleMessageID(messageID)
char *messageID;
{
   dSP;
   int		rc;
   char		*p;
   static char * args[2];
   static char	buf[256];

   if (!PerlFilterActive || perl_filter_cv == NULL)
     return NULL;


   ENTER ;
   SAVETMPS ;

   args[0] = messageID;
   args[1] = 0;
   rc = perl_call_argv ("filter_messageid", G_EVAL|G_SCALAR, args);

   SPAGAIN;

   buf [0] = '\0' ;
   
   if (SvTRUE(GvSV(errgv)))     /* check $@ */
     {
       syslog (L_ERROR,"Perl function filter_messageid died: %s",
               SvPV(GvSV(errgv), na)) ;
       POPs ;
       PerlFilter (FALSE) ;
     }
   else if (rc == 1)
     {
       p = POPp;

       if (p != NULL && *p != '\0')
         {
           strncpy(buf, p, sizeof(buf) - 1);
           buf[sizeof(buf) - 1] = '\0';
         }
     }
 
   PUTBACK;
   FREETMPS;
   LEAVE;

   if (buf[0] != '\0') 
      return buf ;
   return NULL;
}

void
PerlMode(Mode, NewMode, reason)
OPERATINGMODE	Mode, NewMode;
char		*reason;
{
    dSP ;
    HV	*hdr;

    ENTER ;
    SAVETMPS ;
    
    hdr = perl_get_hv("mode", TRUE);

    if (Mode == OMrunning)
        hv_store(hdr, "Mode", 4, newSVpv("running", 0), 0);
    if (Mode == OMpaused)
        hv_store(hdr, "Mode", 4, newSVpv("paused", 0), 0);
    if (Mode == OMthrottled)
        hv_store(hdr, "Mode", 4, newSVpv("throttled", 0), 0);

    if (NewMode == OMrunning)
        hv_store(hdr, "NewMode", 7, newSVpv("running", 0), 0);
    if (NewMode == OMpaused)
        hv_store(hdr, "NewMode", 7, newSVpv("paused", 0), 0);
    if (NewMode == OMthrottled)
        hv_store(hdr, "NewMode", 7, newSVpv("throttled", 0), 0);

    hv_store(hdr, "reason", 6, newSVpv(reason, 0), 0);

    if (perl_get_cv("filter_mode", FALSE) != NULL) {
        perl_call_argv("filter_mode", G_EVAL|G_DISCARD|G_NOARGS, NULL);
        if (SvTRUE(GvSV(errgv))) { /* check $@ */
            syslog (L_ERROR,"Perl function filter_mode died: %s",
                    SvPV(GvSV(errgv), na)) ;
            POPs ;
            PerlFilter (FALSE) ;
        }
    }

    FREETMPS ;
    LEAVE ;
}

#endif /* defined(DO_PERL) */
