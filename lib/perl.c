/*
**  Embedded Perl support for INN.
**
**  Originally written by Christophe Wolfhugel <wolf@pasteur.fr> (although
**  he wouldn't recognize it any more, so don't blame him) and modified,
**  expanded, and tweaked by James Brister, Dave Hayes, and Russ Allbery
**  among others.
**
**  This file contains the Perl linkage shared by both nnrpd and innd.  It
**  assumes Perl 5.004 or later.
*/

#include "portable/system.h"

/* Skip this entire file if DO_PERL (./configure --with-perl) isn't set. */
#if DO_PERL

#    include <fcntl.h>
#    include <syslog.h>

#    include "inn/libinn.h"

#    include <EXTERN.h>
#    include <perl.h>

#    include "ppport.h"
#    include <XSUB.h>

#    include "innperl.h"

/* Provided by DynaLoader but not declared in Perl's header files. */
XS(boot_DynaLoader);

/* Forward declarations. */
void PerlSilence(void);
void PerlUnSilence(void);
void xs_init(pTHX);

/* Forward declarations of XS subs. */
XS(XS_INN_syslog);

/* Whether Perl filtering is currently active. */
bool PerlFilterActive = false;

/* The filter sub called (filter_art or filter_post). */
static CV *perl_filter_cv;

/* The embedded Perl interpreter. */
static PerlInterpreter *PerlCode = NULL;


static void
LogPerl(void)
{
    syslog(L_NOTICE, "SERVER perl filtering %s",
           PerlFilterActive ? "enabled" : "disabled");
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
    char *argv[] = {NULL};
    SV *errsv;

    if (value == PerlFilterActive)
        return true;

    if (!value) {
        /* Execute an end function, if one is defined. */
        if (perl_get_cv("filter_end", false) != NULL) {
            ENTER;
            SAVETMPS;
            /* No need for PUSHMARK(SP) with call_argv(). */
            perl_call_argv("filter_end", G_EVAL | G_DISCARD | G_NOARGS, argv);
            SPAGAIN;
            errsv = ERRSV;
            if (SvTRUE(errsv)) {
                syslog(L_ERROR, "SERVER perl function filter_end died: %s",
                       SvPV_nolen(errsv));
                (void) POPs;
            }
            PUTBACK;
            FREETMPS;
            LEAVE;
        }
        PerlFilterActive = value;
        LogPerl();
        return true;
    } else {
        if (perl_filter_cv == NULL) {
            syslog(L_ERROR, "SERVER perl filter not defined");
            return false;
        } else {
            PerlFilterActive = value;
            LogPerl();
            return true;
        }
    }
}


/*
** Loads a setup Perl module.  startupfile is the name of the file loaded
** one-time at startup.  filterfile is the file containing the filter
** functions which is loaded at startup and at each reload.  function is a
** function name that must be defined after the filterfile file is loaded for
** filtering to be turned on to start with.
*/
void
PERLsetup(char *startupfile, char *filterfile, const char *function)
{
    if (PerlCode == NULL) {
        /* Perl waits on standard input if not called with '-e'. */
        int argc = 3;
        const char *argv_innd[] = {"innd", "-e", "0", NULL};
        char **argv = (char **) argv_innd; /* Cast required by Perl 5.10. */
        char **env = {NULL};
#    ifdef PERL_SYS_INIT3
        PERL_SYS_INIT3(&argc, &argv, &env);
#    endif
        PerlCode = perl_alloc();
        perl_construct(PerlCode);
#    ifdef PERL_EXIT_DESTRUCT_END
        PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
#    endif
        perl_parse(PerlCode, xs_init, argc, argv, env);
    }

    if (startupfile != NULL && filterfile != NULL) {
        char *evalfile = NULL;
        bool failure;
        SV *errsv;
        dSP;

        ENTER;
        SAVETMPS;
        PUSHMARK(SP);
        PUTBACK;

        /* The Perl expression which will be evaluated. */
        xasprintf(&evalfile, "do '%s'", startupfile);

        PerlSilence();
        perl_eval_pv(evalfile, TRUE);
        PerlUnSilence();

        SPAGAIN;

        free(evalfile);
        evalfile = NULL;

        /* Check $@. */
        errsv = ERRSV;
        if (SvTRUE(errsv)) {
            failure = true;
            syslog(L_ERROR, "SERVER perl loading %s failed: %s", startupfile,
                   SvPV_nolen(errsv));
        } else {
            failure = false;
        }

        PUTBACK;
        FREETMPS;
        LEAVE;

        if (failure) {
            PerlFilter(false);
        } else {
            PERLreadfilter(filterfile, function);
        }
    } else {
        PERLreadfilter(filterfile, function);
    }
}


/* Load the perl file FILTERFILE. After it is load check that the given
   function is defined. If yes filtering is turned on. If not it is turned
   off. We remember whether the filter function was defined properly so
   that we can catch when the user tries to turn filtering on without the
   function there. */
int
PERLreadfilter(char *filterfile, const char *function)
{
    dSP;
    char *argv[] = {NULL};
    char *evalfile = NULL;
    bool failure;
    SV *errsv;

    if (perl_get_cv("filter_before_reload", false) != NULL) {
        ENTER;
        SAVETMPS;
        /* No need for PUSHMARK(SP) with call_argv(). */
        perl_call_argv("filter_before_reload", G_EVAL | G_DISCARD | G_NOARGS,
                       argv);
        SPAGAIN;
        /* Check $@. */
        errsv = ERRSV;
        if (SvTRUE(errsv)) {
            failure = true;
            syslog(L_ERROR,
                   "SERVER perl function filter_before_reload died: %s",
                   SvPV_nolen(errsv));
            (void) POPs;
        } else {
            failure = false;
        }
        PUTBACK;
        FREETMPS;
        LEAVE;
        if (failure)
            PerlFilter(false);
    }

    ENTER;
    SAVETMPS;
    PUSHMARK(SP);
    PUTBACK;

    /* The Perl expression which will be evaluated. */
    xasprintf(&evalfile, "do '%s'", filterfile);

    PerlSilence();
    perl_eval_pv(evalfile, TRUE);
    PerlUnSilence();

    SPAGAIN;

    free(evalfile);
    evalfile = NULL;

    /* Check $@. */
    errsv = ERRSV;
    if (SvTRUE(errsv)) {
        failure = true;
        syslog(L_ERROR, "SERVER perl loading %s failed: %s", filterfile,
               SvPV_nolen(errsv));
    } else {
        failure = false;
    }
    PUTBACK;
    FREETMPS;
    LEAVE;
    if (failure) {
        PerlFilter(false);

        /* If the reload failed we don't want the old definition hanging
           around. */

        ENTER;
        SAVETMPS;
        PUSHMARK(SP);
        PUTBACK;

        xasprintf(&evalfile, "undef &%s", function);
        perl_eval_pv(evalfile, TRUE);

        SPAGAIN;

        free(evalfile);
        evalfile = NULL;

        /* Check $@. */
        errsv = ERRSV;
        if (SvTRUE(errsv)) {
            syslog(L_ERROR, "SERVER perl undef &%s failed: %s", function,
                   SvPV_nolen(errsv));
        }
        PUTBACK;
        FREETMPS;
        LEAVE;
    } else if ((perl_filter_cv = perl_get_cv(function, false)) == NULL) {
        PerlFilter(false);
    }

    if (perl_get_cv("filter_after_reload", false) != NULL) {
        ENTER;
        SAVETMPS;
        /* No need for PUSHMARK(SP) with call_argv(). */
        perl_call_argv("filter_after_reload", G_EVAL | G_DISCARD | G_NOARGS,
                       argv);
        SPAGAIN;
        /* Check $@. */
        errsv = ERRSV;
        if (SvTRUE(errsv)) {
            failure = true;
            syslog(L_ERROR,
                   "SERVER perl function filter_after_reload died: %s",
                   SvPV_nolen(errsv));
            (void) POPs;
        } else {
            failure = false;
        }
        PUTBACK;
        FREETMPS;
        LEAVE;
        if (failure) {
            PerlFilter(false);
        }
    }

    /* We try to find an inversion between filter_innd.pl
     * and filter_nnrpd.pl. */
    if (function != NULL) {
        if ((strncmp(function, "filter_art", 10) == 0)
            && (perl_get_cv("filter_post", false) != NULL)) {
            syslog(L_NOTICE, "filter_innd.pl defines a filter_post function"
                             " -- maybe a confusion with filter_nnrpd.pl?");
        }
        if ((strncmp(function, "filter_post", 11) == 0)
            && (perl_get_cv("filter_art", false) != NULL)) {
            syslog(L_NOTICE, "filter_nnrpd.pl defines a filter_art function"
                             " -- maybe a confusion with filter_innd.pl?");
        }
    }

    return (perl_filter_cv != NULL);
}


/*
**  Stops using the Perl filter.
*/
void
PerlClose(void)
{
    if (PerlCode != NULL) {
        perl_destruct(PerlCode);
        perl_free(PerlCode);
#    ifdef PERL_SYS_TERM
        PERL_SYS_TERM();
#    endif
    }
    PerlFilterActive = false;
}

/*
**  Redirects STDOUT/STDERR briefly (otherwise PERL complains to the net
**  connection for NNRPD and that just won't do) -- dave@jetcafe.org
*/
static int savestdout = 0;
static int savestderr = 0;
void
PerlSilence(void)
{
    int newfd;

    /* Save the descriptors */
    if ((savestdout = dup(1)) < 0) {
        syslog(L_ERROR, "SERVER perl silence cant redirect stdout: %m");
        savestdout = 0;
        return;
    }
    if ((savestderr = dup(2)) < 0) {
        syslog(L_ERROR, "SERVER perl silence cant redirect stderr: %m");
        savestdout = 0;
        savestderr = 0;
        return;
    }

    /* Open /dev/null */
    if ((newfd = open("/dev/null", O_WRONLY)) < 0) {
        syslog(L_ERROR, "SERVER perl silence cant open /dev/null: %m");
        savestdout = 0;
        savestderr = 0;
        return;
    }

    /* Redirect descriptors */
    if (dup2(newfd, 1) < 0) {
        syslog(L_ERROR, "SERVER perl silence cant redirect stdout: %m");
        savestdout = 0;
        return;
    }

    if (dup2(newfd, 2) < 0) {
        syslog(L_ERROR, "SERVER perl silence cant redirect stderr: %m");
        savestderr = 0;
        return;
    }
    close(newfd);
}

void
PerlUnSilence(void)
{
    if (savestdout != 0) {
        if (dup2(savestdout, 1) < 0) {
            syslog(L_ERROR, "SERVER perl silence cant restore stdout: %m");
        }
        close(savestdout);
        savestdout = 0;
    }

    if (savestderr != 0) {
        if (dup2(savestderr, 2) < 0) {
            syslog(L_ERROR, "SERVER perl silence cant restore stderr: %m");
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

    loglevel = (const char *) SvPV_nolen(ST(0));
    logmsg = (const char *) SvPV_nolen(ST(1));

    switch (*loglevel) {
    case 'a':
    case 'A':
        priority = LOG_ALERT;
        break;
    case 'c':
    case 'C':
        priority = LOG_CRIT;
        break;
    case 'e':
    case 'E':
        priority = LOG_ERR;
        break;
    case 'w':
    case 'W':
        priority = LOG_WARNING;
        break;
    case 'n':
    case 'N':
        priority = LOG_NOTICE;
        break;
    case 'i':
    case 'I':
        priority = LOG_INFO;
        break;
    case 'd':
    case 'D':
        priority = LOG_DEBUG;
        break;
    default:
        priority = LOG_NOTICE;
    }
    syslog(priority, "filter: %s", logmsg);
    XSRETURN_UNDEF;
}

void
xs_init(pTHX)
{
    dXSUB_SYS;
    inn_newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, "perl.c");
    inn_newXS("INN::syslog", XS_INN_syslog, "perl.c");
}

#endif /* defined(DO_PERL) */
