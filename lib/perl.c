/*  $Id$
**
**  Embedded Perl support for INN.
**
**  Originally written by Christophe Wolfhugel <wolf@pasteur.fr> (although
**  he wouldn't recongize it any more, so don't blame him) and modified,
**  expanded, and tweaked by James Brister, Dave Hayes, and Russ Allbery
**  among others.
**
**  This file contains the Perl linkage shared by both nnrpd and innd.  It
**  assumes Perl 5.004 or later.
*/

#include "config.h"

/* Skip this entire file if DO_PERL (./configure --with-perl) isn't set. */
#if DO_PERL

#include "clibrary.h"
#include <fcntl.h>
#include <syslog.h>

#include "libinn.h"
#include "macros.h"

#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>
#include "ppport.h"

#include "innperl.h"

/* Provided by DynaLoader but not declared in Perl's header files. */
extern void boot_DynaLoader(CV *cv);

/* Forward declarations. */
void    PerlSilence(void);
void    PerlUnSilence(void);
void    xs_init(void);

/* Whether Perl filtering is currently active. */
bool PerlFilterActive = FALSE;

/* The filter sub called (filter_art or filter_post). */
CV *perl_filter_cv;

/* The embedded Perl interpretor. */
static PerlInterpreter *PerlCode;


static void LogPerl(void)
{
   syslog(L_NOTICE, "SERVER perl filtering %s", PerlFilterActive ? "enabled" : "disabled");
}


/*
**  Enable or disable the Perl filter.  Takes the desired state of the filter
**  as an argument and returns success or failure.  Failure to enable
**  indicates that the filter is not defined.
*/
bool
PerlFilter(bool value)
{
    dSP;

    if (value == PerlFilterActive)
        return true;

    if (!value) {
        /* Execute an end function, if one is defined. */
        if (perl_get_cv("filter_end", false) != NULL) {
            ENTER;
            SAVETMPS;
            perl_call_argv("filter_end", G_EVAL | G_DISCARD | G_NOARGS, NULL);
            if (SvTRUE(ERRSV)) {
                syslog (L_ERROR, "SERVER perl function filter_end died: %s",
                        SvPV(ERRSV, PL_na));
                (void) POPs;
            }
            FREETMPS;
            LEAVE;
        }
        PerlFilterActive = value;
        LogPerl();
        return true;
    } else {
        if (perl_filter_cv == NULL) {
            syslog (L_ERROR, "SERVER perl filter not defined");
            return false;
        } else {
            PerlFilterActive = value;
            LogPerl();
            return true;
        }
    }
}

static void PerlParse (void)
{
    const char *argv[] = { "innd",
                           "-e", "sub _load_ { do $_[0] }",
                           "-e", "sub _eval_ { eval $_[0] }",
                           NULL } ;

    /* We can't call 'eval' and 'do' directly for some reason, so we define
       some wrapper functions to give us access. */
        
    perl_parse (PerlCode,xs_init,5,argv,NULL) ;
}



/*
** Loads a setup Perl module. startupfile is the name of the file loaded
** one-time at startup. filterfile is the file containing the filter
** functions which is loaded at startup and at each reload. function is a
** function name that must be defined after the file file is loaded for
** filtering to be turned on to start with.
*/
void PERLsetup (char *startupfile, char *filterfile, const char *function)
{
    if (PerlCode == NULL) {
        PerlCode = perl_alloc();
        perl_construct(PerlCode);
        PerlParse () ;
    }
    
    if (startupfile != NULL && filterfile != NULL) {
        char *argv[2] ;
        int rc ;
        dSP;
    
        ENTER ;
        SAVETMPS ;
    
        argv[0] = startupfile ;
        argv[1] = NULL ;

        PerlSilence();
        rc = perl_call_argv ("_load_",G_DISCARD, argv) ;
        PerlUnSilence();
        
        SPAGAIN ;
        
        if (SvTRUE(ERRSV))     /* check $@ */ {
            syslog(L_ERROR,"SERVER perl loading %s failed: %s",
		   startupfile, SvPV(ERRSV, PL_na)) ;
            PerlFilter (FALSE) ;
    
        } else {
            PERLreadfilter (filterfile,function) ;
        }

        FREETMPS ;
        LEAVE ;
    } else {
        PERLreadfilter (filterfile,function) ;
    }
}


/* Load the perl file FILTERFILE. After it is load check that the give
   function is defined. If yes filtering is turned on. If not it is turned
   off. We remember whether the filter function was defined properly so
   that we can catch when the use tries to turn filtering on without the
   the funciton there. */
int PERLreadfilter(char *filterfile, const char *function)
{
    dSP ;
    char *argv [3] ;
    
    ENTER ;
    SAVETMPS ;
    
    argv[0] = filterfile ;
    argv[1] = NULL ;
    
    if (perl_get_cv("filter_before_reload", FALSE) != NULL)    {
        perl_call_argv("filter_before_reload",G_EVAL|G_DISCARD|G_NOARGS,NULL);
        if (SvTRUE(ERRSV))     /* check $@ */ {
            syslog (L_ERROR,"SERVER perl function filter_before_reload died: %s",
                    SvPV(ERRSV, PL_na)) ;
            (void)POPs ;
            PerlFilter (FALSE) ;
        }
    }

    PerlSilence();
    perl_call_argv ("_load_", 0, argv) ;
    PerlUnSilence();

    if (SvTRUE(ERRSV))     /* check $@ */ {
        syslog (L_ERROR,"SERVER perl loading %s failed: %s",
                filterfile, SvPV(ERRSV, PL_na)) ;
        PerlFilter (FALSE) ;
        
        /* If the reload failed we don't want the old definition hanging
           around. */
        argv[0] = NEW (char,strlen (function) + strlen ("undef &%s")) ;
        sprintf (argv[0],"undef &%s",function) ;
        perl_call_argv ("_eval_",0,argv) ;

        if (SvTRUE(ERRSV))     /* check $@ */ {
            syslog (L_ERROR,"SERVER perl undef &%s failed: %s",
                    function, SvPV(ERRSV, PL_na)) ;
        }
        DISPOSE (argv[0]) ;
    } else if ((perl_filter_cv = perl_get_cv(function, FALSE)) == NULL) {
        PerlFilter (FALSE) ;
    }
    
    if (perl_get_cv("filter_after_reload", FALSE) != NULL) {
        perl_call_argv("filter_after_reload", G_EVAL|G_DISCARD|G_NOARGS, NULL);
        if (SvTRUE(ERRSV))     /* check $@ */ {
            syslog (L_ERROR,"SERVER perl function filter_after_reload died: %s",
                    SvPV(ERRSV, PL_na)) ;
            (void)POPs ;
            PerlFilter (FALSE) ;
        }
    }

    FREETMPS ;
    LEAVE ;

    return (perl_filter_cv != NULL) ;
}


/*
**  Stops using the Perl filter
*/
void PerlClose(void)
{
   perl_destruct(PerlCode);
   perl_free(PerlCode);
   PerlFilterActive = FALSE;
}

/*
**  Redirects STDOUT/STDERR briefly (otherwise PERL complains to the net
**  connection for NNRPD and that just won't do) -- dave@jetcafe.org
*/
static int savestdout = 0;
static int savestderr = 0;
void PerlSilence(void)
{
  int newfd;

  /* Save the descriptors */
  if ( (savestdout = dup(1)) < 0) {
    syslog(L_ERROR,"SERVER perl silence cant redirect stdout: %m");
    savestdout = 0;
    return;
  }
  if ( (savestderr = dup(2)) < 0) {
    syslog(L_ERROR,"SERVER perl silence cant redirect stderr: %m");
    savestdout = 0;
    savestderr = 0;
    return;
  }

  /* Open /dev/null */
  if ((newfd = open("/dev/null",O_WRONLY)) < 0) {
    syslog(L_ERROR,"SERVER perl silence cant open /dev/null: %m");
    savestdout = 0;
    savestderr = 0;
    return;
  }

  /* Redirect descriptors */
  if (dup2(newfd,1) < 0) {
    syslog(L_ERROR,"SERVER perl silence cant redirect stdout: %m");
    savestdout = 0;
    return;
  }
    
  if (dup2(newfd,2) < 0) {
    syslog(L_ERROR,"SERVER perl silence cant redirect stderr: %m");
    savestderr = 0;
    return;
  }
  close(newfd);
}

void PerlUnSilence(void) {
  if (savestdout != 0) {
    if (dup2(savestdout,1) < 0) {
      syslog(L_ERROR,"SERVER perl silence cant restore stdout: %m");
    }
    close(savestdout);
    savestdout = 0;
  }

  if (savestderr != 0) {
    if (dup2(savestderr,2) < 0) {
      syslog(L_ERROR,"SERVER perl silence cant restore stderr: %m");
    }
    close(savestderr);
    savestderr = 0;
  }
}

/*
**  The remainder of this file consists of XS callbacks usable by either
**  innd or nnrpd and initialized automatically when the Perl filter is
**  initialized, as well as the function that initializes them.
*/

/*
**  Log a message via syslog.  Only the first letter of the priority
**  matters, and this function assumes that the controlling program has
**  already done an openlog().  The argument must be a complete message, not
**  a printf-style format.
*/
XS(XS_INN_syslog)
{
    dXSARGS;
    const char *loglevel;
    const char *logmsg;
    int priority;

    if (items != 2)
        croak("Usage: INN::syslog(level, message)");

    loglevel = (const char *) SvPV(ST(0), PL_na);
    logmsg = (const char *) SvPV(ST(1), PL_na);

    switch (*loglevel) {
        default:                priority = LOG_NOTICE;
        case 'a': case 'A':     priority = LOG_ALERT;           break;
        case 'c': case 'C':     priority = LOG_CRIT;            break;
        case 'e': case 'E':     priority = LOG_ERR;             break;
        case 'w': case 'W':     priority = LOG_WARNING;         break;
        case 'n': case 'N':     priority = LOG_NOTICE;          break;
        case 'i': case 'I':     priority = LOG_INFO;            break;
        case 'd': case 'D':     priority = LOG_DEBUG;           break;
    }
    syslog(priority, "filter: %s", logmsg);
    XSRETURN_UNDEF;
}

extern void
xs_init()
{
    dXSUB_SYS;
    newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, "perl.c");
    newXS("INN::syslog", XS_INN_syslog, "perl.c");
}

#endif /* defined(DO_PERL) */
