/*  $Id$
**
**  Embedded Perl support for INN.
**
**  Originally written by Christophe Wolfhugel <wolf@pasteur.fr> (although
**  he wouldn't recongize it any more, so don't blame him) and modified,
**  expanded, and tweaked by James Brister, Dave Hayes, Andrew Gierth, and
**  Russ Allbery among others.
**
**  This file should contain all innd-specific Perl linkage.  Linkage
**  applicable to both innd and nnrpd should go into lib/perl.c instead.
**
**  We are assuming Perl 5.004 or later.
*/

#include "config.h"
#include "clibrary.h"

#include "inn/innconf.h"
#include "nnrpd.h"
#include "paths.h"
#include "post.h"

#include "nntp.h"

/* Skip this entire file if DO_PERL (./configure --with-perl) isn't set. */
#ifdef DO_PERL

#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>
#include "ppport.h"

#include "innperl.h"

extern HEADER	Table[], *EndOfTable;
extern char LogName[];
extern char PERMuser[];

extern char **OtherHeaders;
extern int OtherCount;
extern bool HeadersModified;

extern bool PerlLoaded;

/* #define DEBUG_MODIFY only if you want to see verbose outout */
#ifdef DEBUG_MODIFY
static FILE *flog;
void dumpTable(char *msg);
#endif /* DEBUG_MODIFY */

char *HandleHeaders(char *article)
{
   dSP;
   HEADER	*hp;
   HV		*hdr;
   SV           *body;
   int		rc;
   char		*p, *q;
   static char	buf[256];
   int   i;
   char *s,*t;
   HE            *scan;
   SV            *modswitch;
   int            OtherSize;
   char *argv[] = { NULL };

   if(!PerlLoaded) {
       loadPerl();
   }

   if (!PerlFilterActive)
       return NULL; /* not really necessary */

#ifdef DEBUG_MODIFY
   if ((flog = fopen("/var/news/log/nnrpdperlerrror","a+")) == NULL) {
     syslog(L_ERROR,"Whoops. Can't open error log: %m");
   }
#endif /* DEBUG_MODIFY */
   
   ENTER ;
   SAVETMPS ;
   
   /* Create the Perl Hash */
   hdr = perl_get_hv("hdr", true);
   for (hp = Table; hp < EndOfTable; hp++) {
      if (hp->Body)
         hv_store(hdr, (char *) hp->Name, strlen(hp->Name), newSVpv(hp->Body, 0), 0);
   }
   
   /* Also store other headers */
   OtherSize = OtherCount;
   for (i = 0; i < OtherCount; i++) {
	p = OtherHeaders[i];
        if (p == NULL) {
          syslog (L_ERROR,"Null header number %d copying headers for Perl",i);
          continue;
        }
        s = strchr(p,':');
        if (s == NULL) {
          syslog (L_ERROR,"Bad header copying headers for Perl: '%s'",p);
          continue;
        }
        s++;
        t = (*s == ' ' ? s + 1 : s);
        hv_store(hdr, p, (s - p) - 1, newSVpv(t, 0), 0);
   }
   /* Store user */
   sv_setpv(perl_get_sv("user",true), PERMuser);
   
   /* Store body */
   body = perl_get_sv("body", true);
   sv_setpv(body, article);

   /* Call the filtering function */
   rc = perl_call_argv("filter_post", G_EVAL|G_SCALAR, argv);

   SPAGAIN;

   /* Restore headers */
   modswitch = perl_get_sv("modify_headers",false);
   HeadersModified = false;
   if (SvTRUE(modswitch)) {
     HeadersModified = true;
     i = 0;

#ifdef DEBUG_MODIFY     
     dumpTable("Before mod");
#endif /* DEBUG_MODIFY */

     hv_iterinit(hdr);
     while ((scan = hv_iternext(hdr)) != NULL) {
       /* Get the values */
       p = HePV(scan, PL_na);  
       s = SvPV(HeVAL(scan), PL_na);
#ifdef DEBUG_MODIFY     
       fprintf(flog,"Hash iter: '%s','%s'\n",p,s);
#endif /* DEBUG_MODIFY */

       /* See if it's a table header */
       for (hp = Table; hp < EndOfTable; hp++) {
         if (strncasecmp(p, hp->Name, hp->Size) == 0) {
           char *copy = xstrdup(s);
           HDR_SET(hp - Table, copy);
           hp->Len = TrimSpaces(hp->Value);
           for (q = hp->Value ; ISWHITE(*q) || *q == '\n' ; q++)
             continue;
           hp->Body = q;
           if (hp->Len == 0) {
             free(hp->Value);
             hp->Value = hp->Body = NULL;
           }
           break;
         }
       }
       if (hp != EndOfTable) continue;
       
       /* Add to other headers */
       if (i >= OtherSize - 1) {
         OtherSize += 20;
         OtherHeaders = xrealloc(OtherHeaders, OtherSize * sizeof(char *));
       }
       t = concat(p, ": ", s, (char *) 0);
       OtherHeaders[i++] = t;
     }
     OtherCount = i;
#ifdef DEBUG_MODIFY
     dumpTable("After Mod");
#endif /* DEBUG_MODIFY */
   }

   hv_undef (hdr);
   sv_setsv (body, &PL_sv_undef);

   buf [0] = '\0' ;
   
   if (SvTRUE(ERRSV))     /* check $@ */ {
       syslog (L_ERROR,"Perl function filter_post died: %s",
               SvPV(ERRSV, PL_na)) ;
       (void)POPs ;
       PerlFilter (false) ;
   } else if (rc == 1) {
       p = POPp;
       if (p != NULL && *p != '\0')
           strlcpy(buf, p, sizeof(buf));
   }

   FREETMPS ;
   LEAVE ;
   
   if (buf[0] != '\0') 
      return buf ;
   return NULL;
}

void loadPerl(void) {
    char *path;

    path = concatpath(innconf->pathfilter, _PATH_PERL_FILTER_NNRPD);
    PERLsetup(NULL, path, "filter_post");
    free(path);
    PerlFilter(true);
    PerlLoaded = true;
}

void perlAccess(char *user, struct vector *access_vec) {
  dSP;
  HV              *attribs;
  SV              *sv;
  int             rc, i;
  char            *key, *val, *buffer;

  if (!PerlFilterActive)
    return;

  ENTER;
  SAVETMPS;

  attribs = perl_get_hv("attributes", true);
  hv_store(attribs, "hostname", 8, newSVpv(ClientHost, 0), 0);
  hv_store(attribs, "ipaddress", 9, newSVpv(ClientIpString, 0), 0);
  hv_store(attribs, "port", 4, newSViv(ClientPort), 0);
  hv_store(attribs, "interface", 9, newSVpv(ServerHost, 0), 0);
  hv_store(attribs, "intipaddr", 9, newSVpv(ServerIpString, 0), 0);
  hv_store(attribs, "intport", 7, newSViv(ServerPort), 0);
  hv_store(attribs, "username", 8, newSVpv(user, 0), 0);

  PUSHMARK(SP);

  if (perl_get_cv("access", 0) == NULL) {
    syslog(L_ERROR, "Perl function access not defined");
    Reply("%d Internal Error (3).  Goodbye\r\n", NNTP_ACCESS_VAL);
    ExitWithStats(1, true);
  }

  rc = perl_call_pv("access", G_EVAL|G_ARRAY);

  SPAGAIN;

  if (rc == 0 ) { /* Error occured, same as checking $@ */
    syslog(L_ERROR, "Perl function access died: %s",
           SvPV(ERRSV, PL_na));
    Reply("%d Internal Error (1).  Goodbye\r\n", NNTP_ACCESS_VAL);
    ExitWithStats(1, true);
  }

  if ((rc % 2) != 0) {
    syslog(L_ERROR, "Perl function access returned an odd number of arguments: %i", rc);
    Reply("%d Internal Error (2).  Goodbye\r\n", NNTP_ACCESS_VAL);
    ExitWithStats(1, true);
  }

  vector_resize(access_vec, (rc / 2));

  buffer = xmalloc(BIG_BUFFER);

  for (i = (rc / 2); i >= 1; i--) {
    sv = POPs;
    val = SvPV(sv, PL_na);
    sv = POPs;
    key = SvPV(sv, PL_na);

    strlcpy(buffer, key, BIG_BUFFER);
    strlcat(buffer, ": \"", BIG_BUFFER);
    strlcat(buffer, val, BIG_BUFFER);
    strlcat(buffer, "\"\n", BIG_BUFFER);
 
    vector_add(access_vec, xstrdup(buffer));
  }

  free(buffer);

  PUTBACK;
  FREETMPS;
  LEAVE;

}

void perlAuthInit(void) {
    dSP;
    int             rc;
    
    if (!PerlFilterActive)
	return;

    ENTER;
    SAVETMPS;
    PUSHMARK(SP);
    
    if (perl_get_cv("auth_init", 0) == NULL) {
      syslog(L_ERROR, "Perl function auth_init not defined");
      Reply("%d Internal Error (3).  Goodbye\r\n", NNTP_ACCESS_VAL);
      ExitWithStats(1, true);
    }

    rc = perl_call_pv("auth_init", G_EVAL|G_DISCARD);

    SPAGAIN;


    if (SvTRUE(ERRSV))     /* check $@ */ {
	syslog(L_ERROR, "Perl function authenticate died: %s",
	       SvPV(ERRSV, PL_na));
	Reply("%d Internal Error (1).  Goodbye\r\n", NNTP_ACCESS_VAL);
	ExitWithStats(1, true);
    }

    while (rc--) {
	(void)POPs;
    }

    PUTBACK;
    FREETMPS;
    LEAVE;
    
}

int perlAuthenticate(char *user, char *passwd, char *errorstring, char *newUser) {
    dSP;
    HV              *attribs;
    int             rc;
    char            *p;
    int             code;
    
    if (!PerlFilterActive)
        return NNTP_ACCESS_VAL;

    if (perl_get_cv("authenticate", 0) == NULL) {
        syslog(L_ERROR, "Perl function authenticate not defined");
        Reply("%d Internal Error (3).  Goodbye\r\n", NNTP_ACCESS_VAL);
        ExitWithStats(1, true);
    }

    ENTER;
    SAVETMPS;
    attribs = perl_get_hv("attributes", true);
    hv_store(attribs, "hostname", 8, newSVpv(ClientHost, 0), 0);
    hv_store(attribs, "ipaddress", 9, newSVpv(ClientIpString, 0), 0);
    hv_store(attribs, "port", 4, newSViv(ClientPort), 0);
    hv_store(attribs, "interface", 9, newSVpv(ServerHost, 0), 0);
    hv_store(attribs, "intipaddr", 9, newSVpv(ServerIpString, 0), 0);
    hv_store(attribs, "intport", 7, newSViv(ServerPort), 0);
    hv_store(attribs, "username", 8, newSVpv(user, 0), 0);
    hv_store(attribs, "password", 8, newSVpv(passwd, 0), 0);
    
    PUSHMARK(SP);
    rc = perl_call_pv("authenticate", G_EVAL|G_ARRAY);

    SPAGAIN;

    if (rc == 0 ) { /* Error occured, same as checking $@ */
	syslog(L_ERROR, "Perl function authenticate died: %s",
	       SvPV(ERRSV, PL_na));
	Reply("%d Internal Error (1).  Goodbye\r\n", NNTP_ACCESS_VAL);
	ExitWithStats(1, false);
    }

    if ((rc != 3) && (rc != 2)) {
	syslog(L_ERROR, "Perl function authenticate returned wrong number of results: %d", rc);
	Reply("%d Internal Error (2).  Goodbye\r\n", NNTP_ACCESS_VAL);
	ExitWithStats(1, false);
    }

    if (rc == 3) {
      p = POPp;
      strcpy(newUser, p);
    } 

    p = POPp;
    strcpy(errorstring, p);

    code = POPi;

    if ((code == NNTP_POSTOK_VAL) || (code == NNTP_NOPOSTOK_VAL))
	code = PERMcanpost ? NNTP_POSTOK_VAL : NNTP_NOPOSTOK_VAL;

    if (code == NNTP_AUTH_NEEDED_VAL) 
	PERMneedauth = true;

    hv_undef(attribs);

    PUTBACK;
    FREETMPS;
    LEAVE;
    
    return code;
}

#ifdef DEBUG_MODIFY
void
dumpTable (msg)
char *msg;
{
      HEADER        *hp;
      int   i;

      fprintf(flog,"===BEGIN TABLE DUMP: %s\n",msg);
      
      for (hp = Table; hp < EndOfTable; hp++) {
        fprintf(flog," Name: '%s'",hp->Name); fflush(flog);
        fprintf(flog," Size: '%d'",hp->Size); fflush(flog);
        fprintf(flog," Value: '%s'\n",((hp->Value == NULL) ? "(NULL)" : hp->Value)); fflush(flog);
      }

      for (i=0; i<OtherCount; i++) {
        fprintf(flog,"Extra[%02d]: %s\n",i,OtherHeaders[i]);
      }
      fprintf(flog,"===END TABLE DUMP: %s\n",msg);
}
#endif /* DEBUG_MODIFY */

#endif /* DO_PERL */
