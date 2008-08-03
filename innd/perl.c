/*  $Id$
**
**  Perl filtering support for innd.
**
**  Originally written by Christophe Wolfhugel <wolf@pasteur.fr> (although
**  he wouldn't recognise it anymore so don't blame him) and modified,
**  expanded and tweaked since by James Brister, Jeremy Nixon, Ed Mooring,
**  Russell Vincent, and Russ Allbery.
**
**  This file should contain all innd-specific Perl linkage.  Linkage
**  applicable to both innd and nnrpd should go into lib/perl.c instead.
**
**  We are assuming Perl 5.004 or later.
**
**  Future work:
**
**   - The breakdown between this file, lib/perl.c, and nnrpd/perl.c should
**     be rethought, ideally in the light of supporting multiple filters in
**     different languages.
**
**   - Variable and key names should be standardized between this and nnrpd.
**
**   - The XS code is still calling CC* functions.  The common code between
**     the two control interfaces should be factored out into the rest of
**     innd instead.
**
**   - There's a needless perl_get_cv() call for *every message ID* offered
**     to the server right now.  We need to stash whether that filter is
**     active.
*/

#include "config.h"

/* Skip this entire file if DO_PERL (./configure --with-perl) isn't set. */
#if DO_PERL

#include "clibrary.h"
#include "inn/wire.h"
#include "innd.h"

#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>
#include "ppport.h"

#include "innperl.h"

/* Prototypes for XS functions. */
XS(XS_INN_addhist);
XS(XS_INN_article);
XS(XS_INN_cancel);
XS(XS_INN_filesfor);
XS(XS_INN_havehist);
XS(XS_INN_head);
XS(XS_INN_newsgroup);


/*
**  Run an incoming article through the Perl article filter.  Returns NULL
**  accept the article or a rejection message to reject it.
*/
char *
PLartfilter(const ARTDATA *data, char *artBody, long artLen, int lines)
{
    dSP;
    const ARTHEADER * hp;
    const HDRCONTENT *hc = data->HdrContent;
    HV *        hdr;
    CV *        filter;
    int         i, rc;
    char *      p;
    static SV * body = NULL;
    static char buf[256];

    if (!PerlFilterActive) return NULL;
    filter = perl_get_cv("filter_art", 0);
    if (!filter) return NULL;

    /* Create %hdr and stash a copy of every known header. */
    hdr = perl_get_hv("hdr", 1);
    for (i = 0 ; i < MAX_ARTHEADER ; i++) {
	if (HDR_FOUND(i)) {
	    hp = &ARTheaders[i];
            hv_store(hdr, (char *) hp->Name, hp->Size, newSVpv(HDR(i), 0), 0);
	}
    }

    /* Store the article body.  We don't want to make another copy of it,
       since it could potentially be quite large.  Instead, stash the
       pointer in the static SV * body.  We set LEN to 0 and inc the
       refcount to tell Perl not to free it (either one should be enough).
       Requires 5.004.  In testing, this produced a 17% speed improvement
       over making a copy of the article body for a fairly heavy filter. */
    if (artBody) {
        if (!body) {
            body = newSV(0);
            (void) SvUPGRADE(body, SVt_PV);
        }
        SvPVX(body) = artBody;
        SvCUR_set(body, artLen);
        SvLEN_set(body, 0);
        SvPOK_on(body);
        (void) SvREADONLY_on(body);
        (void) SvREFCNT_inc(body);
        hv_store(hdr, "__BODY__", 8, body, 0);
    }

    hv_store(hdr, "__LINES__", 9, newSViv(lines), 0);

    ENTER;
    SAVETMPS;
    PUSHMARK(SP);
    rc = perl_call_sv((SV *) filter, G_EVAL|G_SCALAR|G_NOARGS);
    SPAGAIN;

    hv_undef(hdr);

    /* Check $@, which will be set if the sub died. */
    buf[0] = '\0';
    if (SvTRUE(ERRSV)) {
        syslog(L_ERROR, "Perl function filter_art died on article %s: %s",
               HDR_FOUND(HDR__MESSAGE_ID) ? HDR(HDR__MESSAGE_ID) : "?",
               SvPV(ERRSV, PL_na));
        (void) POPs;
        PerlFilter(false);
    } else if (rc == 1) {
        p = POPp;
        if (p && *p)
            strlcpy(buf, p, sizeof(buf));
    }

    PUTBACK;
    FREETMPS;
    LEAVE;
    return (buf[0] != '\0') ? buf : NULL;
}


/*
**  Run an incoming message ID from CHECK or IHAVE through the Perl filter.
**  Returns NULL to accept the article or a rejection message to reject it.
*/
char *
PLmidfilter(char *messageID)
{
    dSP;
    CV          *filter;
    int         rc;
    char        *p;
    static char buf[256];

    if (!PerlFilterActive) return NULL;
    filter = perl_get_cv("filter_messageid", 0);
    if (!filter) return NULL;

    /* Pass filter_messageid() the message ID on the Perl stack. */
    ENTER;
    SAVETMPS;
    PUSHMARK(SP);
    XPUSHs(sv_2mortal(newSVpv(messageID, 0)));
    PUTBACK;
    rc = perl_call_sv((SV *) filter, G_EVAL|G_SCALAR);
    SPAGAIN;

    /* Check $@, which will be set if the sub died. */
    buf[0] = '\0';
    if (SvTRUE(ERRSV)) {
        syslog(L_ERROR, "Perl function filter_messageid died on id %s: %s",
               messageID, SvPV(ERRSV, PL_na));
        (void) POPs;
        PerlFilter(false);
    } else if (rc == 1) {
        p = POPp;
        if (p && *p)
            strlcpy(buf, p, sizeof(buf));
    }
    
    PUTBACK;
    FREETMPS;
    LEAVE;
    return (buf[0] != '\0') ? buf : NULL;
}


/*
**  Call a Perl sub on any change in INN's mode, passing in the old and new
**  mode and the reason.
*/
void
PLmode(OPERATINGMODE Mode, OPERATINGMODE NewMode, char *reason)
{
    dSP;
    HV          *mode;
    CV          *filter;

    filter = perl_get_cv("filter_mode", 0);
    if (!filter) return;

    /* Current mode goes into $mode{Mode}, new mode in $mode{NewMode}, and
       the reason in $mode{reason}. */
    mode = perl_get_hv("mode", 1);

    if (Mode == OMrunning)
        hv_store(mode, "Mode", 4, newSVpv("running", 7), 0);
    if (Mode == OMpaused)
        hv_store(mode, "Mode", 4, newSVpv("paused", 6), 0);
    if (Mode == OMthrottled)
        hv_store(mode, "Mode", 4, newSVpv("throttled", 9), 0);
    if (Mode == OMshutdown)
        hv_store(mode, "Mode", 4, newSVpv("shutdown", 8), 0);

    if (NewMode == OMrunning)
        hv_store(mode, "NewMode", 7, newSVpv("running", 7), 0);
    if (NewMode == OMpaused)
        hv_store(mode, "NewMode", 7, newSVpv("paused", 6), 0);
    if (NewMode == OMthrottled)
        hv_store(mode, "NewMode", 7, newSVpv("throttled", 9), 0);
    if (NewMode == OMshutdown)
        hv_store(mode, "NewMode", 7, newSVpv("shutdown", 8), 0);

    hv_store(mode, "reason", 6, newSVpv(reason, 0), 0);

    PUSHMARK(SP);
    perl_call_sv((SV *) filter, G_EVAL|G_DISCARD|G_NOARGS);

    /* Check $@, which will be set if the sub died. */
    if (SvTRUE(ERRSV)) {
        syslog(L_ERROR, "Perl function filter_mode died: %s",
                SvPV(ERRSV, PL_na));
        (void) POPs;
        PerlFilter(false);
    }
}


/*
**  Called by CCmode, this returns the Perl filter statistics if a Perl
**  function to generate such statistics has been defined, or NULL otherwise.
**  If a string is returned, it's in newly allocated memory that must be freed
**  by the caller.
*/
char *
PLstats(void)
{
    dSP;
    char *argv[] = { NULL };
    
    if (perl_get_cv("filter_stats", false) == NULL)
        return NULL;
    else {
        char *stats = NULL;
        char *result;

	ENTER;
	SAVETMPS;
	perl_call_argv("filter_stats", G_EVAL | G_NOARGS, argv);
	SPAGAIN;
        result = POPp;
        if (result != NULL && *result)
            stats = xstrdup(result);
	PUTBACK;
	FREETMPS;
	LEAVE;

        return stats;
    }
}


/*
**  The remainder of this file are XS callbacks visible to embedded Perl
**  code to perform various innd functions.  They were originally written by
**  Ed Mooring (mooring@acm.org) on May 14, 1998, and have since been split
**  between this file and lib/perl.c (which has the ones that can also be
**  used in nnrpd).  The function that registers them at startup is at the
**  end.
*/

/*
**  Add an entry to history.  Takes message ID and optionally arrival,
**  article, and expire times and storage API token.  If the times aren't
**  given, they default to now.  If the token isn't given, that field will
**  be left empty.  Returns boolean success.
*/
XS(XS_INN_addhist)
{
    dXSARGS;
    int         i;
    char        tbuff[32];
    char*       parambuf[6];

    /* Suppress warnings for the mandatory XS argument. */
    cv = cv;

    if (items < 1 || items > 5)
        croak("Usage INN::addhist(msgid,[arrival,articletime,expire,token])");

    for (i = 0; i < items; i++)
        parambuf[i] = (char *) SvPV(ST(0), PL_na);

    /* If any of the times are missing, they should default to now. */
    if (i < 4) {
        snprintf(tbuff, sizeof(tbuff), "%ld", (long) time(NULL));
        for (; i < 4; i++)
            parambuf[i] = tbuff;
    }

    /* The token defaults to an empty string. */
    if (i == 4)
        parambuf[4] = (char *) "";

    parambuf[5] = NULL;

    /* CCaddhist returns NULL on success. */
    if (CCaddhist(parambuf))
        XSRETURN_NO;
    else
        XSRETURN_YES;
}


/*
**  Takes the message ID of an article and returns the full article as a
**  string or undef if the article wasn't found.  It will be converted from
**  wire format to native format.  Note that this call isn't particularly
**  optimized or cheap.
*/
XS(XS_INN_article)
{
    dXSARGS;
    char *      msgid;
    TOKEN       token;
    ARTHANDLE * art;
    char *      p;
    size_t      len;

    /* Suppress warnings for the mandatory XS argument. */
    cv = cv;

    if (items != 1)
	croak("Usage: INN::article(msgid)");

    /* Get the article token from the message ID and the history file. */
    msgid = (char *) SvPV(ST(0), PL_na);
    if (!HISlookup(History, msgid, NULL, NULL, NULL, &token)) XSRETURN_UNDEF;

    /* Retrieve the article and convert it from wire format. */
    art = SMretrieve(token, RETR_ALL);
    if (art == NULL) XSRETURN_UNDEF;
    p = wire_to_native(art->data, art->len, &len);
    SMfreearticle(art);

    /* Push a copy of the article onto the Perl stack, free our temporary
       memory allocation, and return the article to Perl. */
    ST(0) = sv_2mortal(newSVpv(p, len));
    free(p);
    XSRETURN(1);
}


/*
**  Cancel a message by message ID; returns boolean success.  Equivalent to
**  ctlinnd cancel <message>.
*/
XS(XS_INN_cancel)
{
    dXSARGS;
    char        *msgid;
    char        *parambuf[2];

    /* Suppress warnings for the mandatory XS argument. */
    cv = cv;

    if (items != 1)
        croak("Usage: INN::cancel(msgid)");

    msgid = (char *) SvPV(ST(0), PL_na);
    parambuf[0] = msgid;
    parambuf[1] = NULL;

    /* CCcancel returns NULL on success. */
    if (CCcancel(parambuf))
        XSRETURN_NO;
    else
        XSRETURN_YES;
}


/*
**  Return the files for a given message ID, taken from the history file.
**  This function should really be named INN::token() and probably will be
**  some day.
*/
XS(XS_INN_filesfor)
{
    dXSARGS;
    char        *msgid;
    TOKEN       token;

    /* Suppress warnings for the mandatory XS argument. */
    cv = cv;

    if (items != 1)
        croak("Usage: INN::filesfor(msgid)");

    msgid = (char *) SvPV(ST(0), PL_na);
    if (HISlookup(History, msgid, NULL, NULL, NULL, &token)) {
        XSRETURN_PV(TokenToText(token));
    } else {
        XSRETURN_UNDEF;
    }
}


/*
**  Whether message ID is in the history file; returns boolean.
*/
XS(XS_INN_havehist)
{
    dXSARGS;
    char        *msgid;

    /* Suppress warnings for the mandatory XS argument. */
    cv = cv;

    if (items != 1)
        croak("Usage: INN::havehist(msgid)");

    msgid = (char *) SvPV(ST(0), PL_na);
    if (HIScheck(History, msgid))
        XSRETURN_YES;
    else
        XSRETURN_NO;
}


/*
**  Takes the message ID of an article and returns the article headers as
**  a string or undef if the article wasn't found.  Each line of the header
**  will end with \n.
*/
XS(XS_INN_head)
{
    dXSARGS;
    char *      msgid;
    TOKEN       token;
    ARTHANDLE * art;
    char *      p;
    size_t      len;

    /* Suppress warnings for the mandatory XS argument. */
    cv = cv;

    if (items != 1)
        croak("Usage: INN::head(msgid)");

    /* Get the article token from the message ID and the history file. */
    msgid = (char *) SvPV(ST(0), PL_na);
    if (!HISlookup(History, msgid, NULL, NULL, NULL, &token)) XSRETURN_UNDEF;

    /* Retrieve the article header and convert it from wire format. */
    art = SMretrieve(token, RETR_HEAD);
    if (art == NULL) XSRETURN_UNDEF;
    p = wire_to_native(art->data, art->len, &len);
    SMfreearticle(art);

    /* Push a copy of the article header onto the Perl stack, free our
       temporary memory allocation, and return the header to Perl. */
    ST(0) = sv_2mortal(newSVpv(p, len));
    free(p);
    XSRETURN(1);
}


/*
**  Returns the active file flag for a newsgroup or undef if it isn't in the
**  active file.
*/
XS(XS_INN_newsgroup)
{
    dXSARGS;
    char *      newsgroup;
    NEWSGROUP * ngp;
    char *      end;
    int         size;

    /* Suppress warnings for the mandatory XS argument. */
    cv = cv;

    if (items != 1)
        croak("Usage: INN::newsgroup(group)");
    newsgroup = (char *) SvPV(ST(0), PL_na);

    ngp = NGfind(newsgroup);
    if (!ngp) {
        XSRETURN_UNDEF;
    } else {
        /* ngp->Rest is newline-terminated; find the end. */
        end = strchr(ngp->Rest, '\n');
        if (end == NULL) {
            size = strlen(ngp->Rest);
        } else {
            size = end - ngp->Rest;
        }
        ST(0) = sv_2mortal(newSVpv(ngp->Rest, size));
        XSRETURN(1);
    }
}


/*
**  Initialize the XS callbacks defined in this file.
*/
void
PLxsinit(void)
{
    inn_newXS("INN::addhist", XS_INN_addhist, "perl.c");
    inn_newXS("INN::article", XS_INN_article, "perl.c");
    inn_newXS("INN::cancel", XS_INN_cancel, "perl.c");
    inn_newXS("INN::havehist", XS_INN_havehist, "perl.c");
    inn_newXS("INN::head", XS_INN_head, "perl.c");
    inn_newXS("INN::newsgroup", XS_INN_newsgroup, "perl.c");
    inn_newXS("INN::filesfor", XS_INN_filesfor, "perl.c");
}

#endif /* defined(DO_PERL) */
