/* -*- c -*-
 *
 * Author:      Christophe Wolfhugel <wolf@pasteur.fr>
 *		(although he wouldn't recognise it anymore so don't blame him)
 * File:        perl.c
 * RCSId:       $Id$
 * Description: Perl hooks for libinn.a
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
#include "logging.h"


#if defined(DO_PERL)

#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>

#include "macros.h"

extern void xs_init    _((void));
extern void boot_DynaLoader _((CV* cv));

int	PerlFilterActive = FALSE;

static PerlInterpreter	*PerlCode;
CV *perl_filter_cv ;                 /* filter_art or filter_post holder */
extern char	LogName[];

void
LogPerl()
{
   syslog(L_NOTICE, "%s perl filtering %s", LogName, PerlFilterActive ? "enabled" : "disabled");
}

void
PerlFilter(value)
  BOOL value ;
{
    dSP;

    ENTER ;
    SAVETMPS ;
    
    /* Execute an end function */
    if (PerlFilterActive && !value) {
        if (perl_get_cv("filter_end", FALSE) != NULL) {
            perl_call_argv("filter_end", G_EVAL|G_DISCARD|G_NOARGS, NULL);
            if (SvTRUE(GvSV(errgv)))     /* check $@ */ {
                syslog (L_ERROR,"%s perl function filter_end died: %s",
                        LogName, SvPV(GvSV(errgv), na)) ;
                POPs ;
            }
        } else {
            PerlFilterActive = value ;
            LogPerl () ;
        }
    } else if (!PerlFilterActive && value) { /* turning on */
        if (perl_filter_cv == NULL) {
            syslog (L_ERROR,"%s perl filter not defined", LogName) ;
        } else {
            PerlFilterActive = value ;
            LogPerl () ;
        }
    }
    
    FREETMPS ;
    LEAVE ;
}

void
PerlParse ()
{
    char *argv[] = { "innd",
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
int
PERLsetup (startupfile, filterfile, function)
    char *startupfile, *filterfile, *function;
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
        
        rc = perl_call_argv ("_load_",G_DISCARD, argv) ;
        
        SPAGAIN ;
        
        if (SvTRUE(GvSV(errgv)))     /* check $@ */ {
            syslog (L_ERROR,"%s perl loading %s failed: %s",
                    LogName, startupfile, SvPV(GvSV(errgv), na)) ;
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
int
PERLreadfilter(filterfile, function)
  char  *filterfile, *function ;
{
    dSP ;
    char *argv [3] ;
    
    ENTER ;
    SAVETMPS ;
    
    argv[0] = filterfile ;
    argv[1] = NULL ;
    
    if (perl_get_cv("filter_before_reload", FALSE) != NULL)    {
        perl_call_argv("filter_before_reload",G_EVAL|G_DISCARD|G_NOARGS,NULL);
        if (SvTRUE(GvSV(errgv)))     /* check $@ */ {
            syslog (L_ERROR,"%s perl function filter_before_reload died: %s",
                    LogName, SvPV(GvSV(errgv), na)) ;
            POPs ;
            PerlFilter (FALSE) ;
        }
    }

    perl_call_argv ("_load_", 0, argv) ;

    if (SvTRUE(GvSV(errgv)))     /* check $@ */ {
        syslog (L_ERROR,"%s perl loading %s failed: %s",
                LogName, filterfile, SvPV(GvSV(errgv), na)) ;
        PerlFilter (FALSE) ;
        
        /* If the reload failed we don't want the old definition hanging
           around. */
        argv[0] = NEW (char,strlen (function) + strlen ("undef &%s")) ;
        sprintf (argv[0],"undef &%s",function) ;
        perl_call_argv ("_eval_",0,argv) ;

        if (SvTRUE(GvSV(errgv)))     /* check $@ */ {
            syslog (L_ERROR,"%s perl undef &%s failed: %s",
                    LogName, function, SvPV(GvSV(errgv), na)) ;
        }
        DISPOSE (argv[0]) ;
    } else if ((perl_filter_cv = perl_get_cv(function, FALSE)) == NULL) {
        PerlFilter (FALSE) ;
    }
    
    if (perl_get_cv("filter_after_reload", FALSE) != NULL) {
        perl_call_argv("filter_after_reload", G_EVAL|G_DISCARD|G_NOARGS, NULL);
        if (SvTRUE(GvSV(errgv)))     /* check $@ */ {
            syslog (L_ERROR,"%s perl function filter_after_reload died: %s",
                    LogName, SvPV(GvSV(errgv), na)) ;
            POPs ;
            PerlFilter (FALSE) ;
        }
    }

    FREETMPS ;
    LEAVE ;

    return (perl_filter_cv != NULL) ;
}


/*
** Stops using the Perl filter
*/
void
PerlClose()
{
   perl_destruct(PerlCode);
   perl_free(PerlCode);
   PerlFilterActive = FALSE;
}

extern void xs_init()
{
    char * file = __FILE__;
    dXSUB_SYS;

    newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);
}


#endif /* defined(DO_PERL) */

