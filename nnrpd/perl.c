/*
**  Embedded Perl support for INN.
**
**  Originally written by Christophe Wolfhugel <wolf@pasteur.fr> (although
**  he wouldn't recognize it any more, so don't blame him) and modified,
**  expanded, and tweaked by James Brister, Dave Hayes, Andrew Gierth, and
**  Russ Allbery among others.
**
**  This file should contain all innd-specific Perl linkage.  Linkage
**  applicable to both innd and nnrpd should go into lib/perl.c instead.
**
**  We are assuming Perl 5.004 or later.
*/

#include "portable/system.h"

#include "inn/innconf.h"
#include "inn/nntp.h"
#include "inn/paths.h"
#include "nnrpd.h"
#include "post.h"

/* Skip this entire file if DO_PERL (./configure --with-perl) isn't set. */
#ifdef DO_PERL

#    include <EXTERN.h>
#    include <perl.h>

#    include "ppport.h"
#    include <XSUB.h>

#    include "innperl.h"


static bool HeadersModified;

/* #define DEBUG_MODIFY only if you want to see verbose output. */
#    ifdef DEBUG_MODIFY
static FILE *flog;
void dumpTable(const char *msg);
#    endif /* DEBUG_MODIFY */

char *
HandleHeaders(char *article)
{
    dSP;
    HEADER *hp;
    HV *attribs;
    HV *hdr;
    SV *body;
    int rc;
    char *p, *q;
    static char buf[256];
    size_t i;
    size_t len;
    char *s, *t;
    HE *scan;
    SV *modswitch;
    size_t OtherSize;
    char *argv[] = {NULL};
    bool failure;
    SV *errsv;

    if (!PerlLoaded) {
        loadPerl();
    }

    if (!PerlFilterActive)
        return NULL; /* Not really necessary. */

#    ifdef DEBUG_MODIFY
    if ((flog = fopen("/var/news/log/nnrpdperlerrror", "a+")) == NULL) {
        syslog(L_ERROR, "Whoops.  Can't open error log: %m");
    }
#    endif /* DEBUG_MODIFY */

    ENTER;
    SAVETMPS;

    /* Create the Perl attributes hash. */
    attribs = perl_get_hv("attributes", true);
    (void) hv_store(attribs, "hostname", 8, newSVpv(Client.host, 0), 0);
    (void) hv_store(attribs, "ipaddress", 9, newSVpv(Client.ip, 0), 0);
    (void) hv_store(attribs, "port", 4, newSViv(Client.port), 0);
    (void) hv_store(attribs, "interface", 9, newSVpv(Client.serverhost, 0), 0);
    (void) hv_store(attribs, "intipaddr", 9, newSVpv(Client.serverip, 0), 0);
    (void) hv_store(attribs, "intport", 7, newSViv(Client.serverport), 0);

    /* Create the Perl header hash. */
    hdr = perl_get_hv("hdr", true);
    for (hp = Table; hp < EndOfTable; hp++) {
        if (hp->Body)
            (void) hv_store(hdr, (char *) hp->Name, hp->Size,
                            newSVpv(hp->Body, 0), 0);
    }

    /* Also store other headers. */
    OtherSize = OtherCount;
    for (i = 0; i < OtherCount; i++) {
        p = OtherHeaders[i];
        if (p == NULL) {
            syslog(L_ERROR, "Null header number %lu copying headers for Perl",
                   (unsigned long) i);
            continue;
        }
        s = strchr(p, ':');
        if (s == NULL) {
            syslog(L_ERROR, "Bad header copying headers for Perl: '%s'", p);
            continue;
        }
        s++;
        t = (*s == ' ' ? s + 1 : s);
        (void) hv_store(hdr, p, (s - p) - 1, newSVpv(t, 0), 0);
    }

    /* Store user. */
    sv_setpv(perl_get_sv("user", true), PERMuser);

    /* Store body. */
    body = perl_get_sv("body", true);
    sv_setpv(body, article);

    /* Call the filtering function. */
    /* No need for PUSHMARK(SP) with call_argv(). */
    rc = perl_call_argv("filter_post", G_EVAL | G_SCALAR, argv);

    SPAGAIN;

    /* Restore headers if they have just been modified by the filter. */
    modswitch = perl_get_sv("modify_headers", false);
    HeadersModified = false;
    if (SvTRUE(modswitch)) {
        HeadersModified = true;
        i = 0;

#    ifdef DEBUG_MODIFY
        dumpTable("Before mod");
#    endif /* DEBUG_MODIFY */

        hv_iterinit(hdr);
        while ((scan = hv_iternext(hdr)) != NULL) {
            /* Get the values.  We replace the known ones with these
             * new values.  Also, silent a warning caused by the internal Perl
             * SvFLAGS macro used by HePV(), which takes care of the cast. */
#    if defined(__llvm__) || defined(__clang__)
#        pragma GCC diagnostic ignored "-Wcast-align"
#    endif
            p = HePV(scan, len);
#    if defined(__llvm__) || defined(__clang__)
#        pragma GCC diagnostic warning "-Wcast-align"
#    endif
            s = SvPV(HeVAL(scan), PL_na);
#    ifdef DEBUG_MODIFY
            fprintf(flog, "Hash iter: '%s','%s'\n", p, s);
#    endif /* DEBUG_MODIFY */

            /* See if it is a table header. */
            for (hp = Table; hp < EndOfTable; hp++) {
                if (strcasecmp(p, hp->Name) == 0) {
                    char *copy = xstrdup(s);
                    HDR_SET(hp - Table, copy);
                    hp->Len = TrimSpaces(hp->Value);
                    for (q = hp->Value; ISWHITE(*q) || *q == '\n'; q++)
                        continue;
                    hp->Body = q;
                    if (hp->Len == 0) {
                        free(hp->Value);
                        hp->Value = hp->Body = NULL;
                    }
                    break;
                }
            }
            if (hp != EndOfTable)
                continue;

            /* Add to other header fields if not empty. */
            if (TrimSpaces(s) > 0) {
                if (i >= OtherSize - 1) {
                    OtherSize += 20;
                    OtherHeaders =
                        xrealloc(OtherHeaders, OtherSize * sizeof(char *));
                }
                t = concat(p, ": ", s, (char *) 0);
                OtherHeaders[i++] = t;
            }
        }
        OtherCount = i;
#    ifdef DEBUG_MODIFY
        dumpTable("After mod");
#    endif /* DEBUG_MODIFY */
    }

    hv_undef(attribs);
    hv_undef(hdr);
    sv_setsv(body, &PL_sv_undef);

    buf[0] = '\0';

    /* Check $@. */
    errsv = ERRSV;
    if (SvTRUE(errsv)) {
        failure = true;
        syslog(L_ERROR, "Perl function filter_post died: %s",
               SvPV(errsv, PL_na));
        (void) POPs;
    } else {
        failure = false;
        if (rc == 1) {
            p = POPp;
            if (p != NULL && *p != '\0')
                strlcpy(buf, p, sizeof(buf));
        }
    }

    PUTBACK;
    FREETMPS;
    LEAVE;

    if (failure)
        PerlFilter(false);

    if (buf[0] != '\0')
        return buf;

#    ifdef DEBUG_MODIFY
    fclose(flog);
#    endif /* DEBUG_MODIFY */

    return NULL;
}


void
loadPerl(void)
{
    char *path;

    path = concatpath(innconf->pathfilter, INN_PATH_PERL_FILTER_NNRPD);
    PERLsetup(NULL, path, "filter_post");
    free(path);
    PerlFilter(true);
    PerlLoaded = true;
}


void
perlAccess(char *user, struct vector *access_vec)
{
    dSP;
    HV *attribs;
    SV *sv;
    int rc, i;
    char *key, *val, *buffer;
    SV *errsv;

    if (!PerlFilterActive)
        return;

    ENTER;
    SAVETMPS;

    attribs = perl_get_hv("attributes", true);
    (void) hv_store(attribs, "hostname", 8, newSVpv(Client.host, 0), 0);
    (void) hv_store(attribs, "ipaddress", 9, newSVpv(Client.ip, 0), 0);
    (void) hv_store(attribs, "port", 4, newSViv(Client.port), 0);
    (void) hv_store(attribs, "interface", 9, newSVpv(Client.serverhost, 0), 0);
    (void) hv_store(attribs, "intipaddr", 9, newSVpv(Client.serverip, 0), 0);
    (void) hv_store(attribs, "intport", 7, newSViv(Client.serverport), 0);
    (void) hv_store(attribs, "username", 8, newSVpv(user, 0), 0);

    PUSHMARK(SP);
    PUTBACK;

    if (perl_get_cv("access", 0) == NULL) {
        syslog(L_ERROR, "Perl function access not defined");
        Reply("%d Internal error (3).  Goodbye!\r\n", NNTP_FAIL_TERMINATING);
        ExitWithStats(1, true);
    }

    rc = perl_call_pv("access", G_EVAL | G_ARRAY);

    SPAGAIN;

    if (rc == 0) { /* Error occured, same as checking $@. */
        errsv = ERRSV;
        syslog(L_ERROR, "Perl function access died: %s", SvPV(errsv, PL_na));
        Reply("%d Internal error (1).  Goodbye!\r\n", NNTP_FAIL_TERMINATING);
        ExitWithStats(1, true);
    }

    if ((rc % 2) != 0) {
        syslog(L_ERROR,
               "Perl function access returned an odd number of arguments: %i",
               rc);
        Reply("%d Internal error (2).  Goodbye!\r\n", NNTP_FAIL_TERMINATING);
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

        vector_add(access_vec, buffer);
    }

    free(buffer);
    hv_undef(attribs);

    PUTBACK;
    FREETMPS;
    LEAVE;
}


void
perlAuthInit(void)
{
    dSP;
    int rc;
    SV *errsv;

    if (!PerlFilterActive)
        return;

    if (perl_get_cv("auth_init", 0) == NULL) {
        return;
    }

    ENTER;
    SAVETMPS;
    PUSHMARK(SP);
    PUTBACK;

    rc = perl_call_pv("auth_init", G_EVAL | G_DISCARD);

    SPAGAIN;

    errsv = ERRSV;
    if (SvTRUE(errsv)) { /* Check $@. */
        syslog(L_ERROR, "Perl function authenticate died: %s",
               SvPV(errsv, PL_na));
        Reply("%d Internal error (1).  Goodbye!\r\n", NNTP_FAIL_TERMINATING);
        ExitWithStats(1, true);
    }

    while (rc--) {
        (void) POPs;
    }

    PUTBACK;
    FREETMPS;
    LEAVE;
}


void
perlAuthenticate(char *user, char *passwd, int *code, char *errorstring,
                 char *newUser)
{
    dSP;
    HV *attribs;
    int rc;
    char *p;
    SV *errsv;

    if (!PerlFilterActive)
        *code = NNTP_FAIL_AUTHINFO_BAD;

    if (perl_get_cv("authenticate", 0) == NULL) {
        syslog(L_ERROR, "Perl function authenticate not defined");
        Reply("%d Internal error (3).  Goodbye!\r\n", NNTP_FAIL_TERMINATING);
        ExitWithStats(1, true);
    }

    ENTER;
    SAVETMPS;
    attribs = perl_get_hv("attributes", true);
    (void) hv_store(attribs, "hostname", 8, newSVpv(Client.host, 0), 0);
    (void) hv_store(attribs, "ipaddress", 9, newSVpv(Client.ip, 0), 0);
    (void) hv_store(attribs, "port", 4, newSViv(Client.port), 0);
    (void) hv_store(attribs, "interface", 9, newSVpv(Client.serverhost, 0), 0);
    (void) hv_store(attribs, "intipaddr", 9, newSVpv(Client.serverip, 0), 0);
    (void) hv_store(attribs, "intport", 7, newSViv(Client.serverport), 0);
    (void) hv_store(attribs, "username", 8, newSVpv(user, 0), 0);
    (void) hv_store(attribs, "password", 8, newSVpv(passwd, 0), 0);

    PUSHMARK(SP);
    PUTBACK;
    rc = perl_call_pv("authenticate", G_EVAL | G_ARRAY);

    SPAGAIN;

    if (rc == 0) { /* Error occurred, same as checking $@. */
        errsv = ERRSV;
        syslog(L_ERROR, "Perl function authenticate died: %s",
               SvPV(errsv, PL_na));
        Reply("%d Internal error (1).  Goodbye!\r\n", NNTP_FAIL_TERMINATING);
        ExitWithStats(1, false);
    }

    if ((rc != 3) && (rc != 2)) {
        syslog(
            L_ERROR,
            "Perl function authenticate returned wrong number of results: %d",
            rc);
        Reply("%d Internal error (2).  Goodbye!\r\n", NNTP_FAIL_TERMINATING);
        ExitWithStats(1, false);
    }

    /* FIXME: Change the structure of the code so that we don't have to rely
     * on keeping these sizes in sync with the buffers that are passed into
     * this function. */
    if (rc == 3) {
        p = POPp;
        strlcpy(newUser, p, BIG_BUFFER);
    }

    p = POPp;
    strlcpy(errorstring, p, BIG_BUFFER);

    *code = POPi;

    hv_undef(attribs);

    PUTBACK;
    FREETMPS;
    LEAVE;
}


#    ifdef DEBUG_MODIFY
void
dumpTable(const char *msg)
{
    HEADER *hp;
    size_t i;

    fprintf(flog, "===BEGIN TABLE DUMP: %s\n", msg);

    for (hp = Table; hp < EndOfTable; hp++) {
        fprintf(flog, " Name: '%s'", hp->Name);
        fflush(flog);
        fprintf(flog, " Size: '%d'", hp->Size);
        fflush(flog);
        fprintf(flog, " Value: '%s'\n",
                ((hp->Value == NULL) ? "(NULL)" : hp->Value));
        fflush(flog);
    }

    for (i = 0; i < OtherCount; i++) {
        fprintf(flog, "Extra[%02lu]: %s\n", i, OtherHeaders[i]);
    }
    fprintf(flog, "===END TABLE DUMP: %s\n", msg);
}
#    endif /* DEBUG_MODIFY */

#endif /* DO_PERL */
