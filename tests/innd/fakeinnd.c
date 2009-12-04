/* $Id$ */
/* Global symbols and tables required for innd-related files to link. */

/* This file is required in order to test various portions of innd from
   outside of innd itself.  We have to omit innd.c since it defines main, so
   we have to provide everything that innd.c provides.  This is also a good
   list of globals in innd that need to be dealt with in some other way. */

#define DEFINE_DATA 1
#include "../../innd/innd.h"

/* Global variables defined in innd.c. */
bool Debug = false;
bool NNRPTracing = false;
bool StreamingOff = false;
bool Tracing = false;
bool DoCancels = true;
char LogName[] = "SERVER";
int ErrorCount = IO_ERROR_COUNT;
OPERATINGMODE Mode = OMrunning;
int RemoteLimit = REMOTELIMIT;
time_t RemoteTimer = REMOTETIMER;
int RemoteTotal = REMOTETOTAL;
bool ThrottledbyIOError = false;
bool BufferedLogs = true;
FILE *Log = NULL;
FILE *Errlog = NULL;

/* We have to copy this here.  Bleh.  We only need to care about headers up to
   message ID and Path, but they have to be in the same order as in innd.c and
   the whole array has to be the same size.  Eventually, this will get moved
   into a separate file. */
#define ARTHEADERINIT(name, type) { name, type, sizeof(name) - 1 }
const ARTHEADER ARTheaders[] = {
    ARTHEADERINIT("Approved",              HTstd),
    ARTHEADERINIT("Control",               HTstd),
    ARTHEADERINIT("Date",                  HTreq),
    ARTHEADERINIT("Distribution",          HTstd),
    ARTHEADERINIT("Expires",               HTstd),
    ARTHEADERINIT("From",                  HTreq),
    ARTHEADERINIT("Lines",                 HTstd),
    ARTHEADERINIT("Message-ID",            HTreq),
    ARTHEADERINIT("Newsgroups",            HTreq),
    ARTHEADERINIT("Path",                  HTreq),
    ARTHEADERINIT("Reply-To",              HTstd),
    ARTHEADERINIT("Sender",                HTstd),
    ARTHEADERINIT("Subject",               HTreq),
    ARTHEADERINIT("Supersedes",            HTstd),
    ARTHEADERINIT("Bytes",                 HTstd),
    ARTHEADERINIT("Also-Control",          HTobs),
    ARTHEADERINIT("References",            HTstd),
    ARTHEADERINIT("Xref",                  HTsav),
    ARTHEADERINIT("Keywords",              HTstd),
    ARTHEADERINIT("X-Trace",               HTstd),
    ARTHEADERINIT("Date-Received",         HTobs),
    ARTHEADERINIT("Posted",                HTobs),
    ARTHEADERINIT("Posting-Version",       HTobs),
    ARTHEADERINIT("Received",              HTobs),
    ARTHEADERINIT("Relay-Version",         HTobs),
    ARTHEADERINIT("NNTP-Posting-Host",     HTstd),
    ARTHEADERINIT("Followup-To",           HTstd),
    ARTHEADERINIT("Organization",          HTstd),
    ARTHEADERINIT("Content-Type",          HTstd),
    ARTHEADERINIT("Content-Base",          HTstd),
    ARTHEADERINIT("Content-Disposition",   HTstd),
    ARTHEADERINIT("X-Newsreader",          HTstd),
    ARTHEADERINIT("X-Mailer",              HTstd),
    ARTHEADERINIT("X-Newsposter",          HTstd),
    ARTHEADERINIT("X-Cancelled-By",        HTstd),
    ARTHEADERINIT("X-Canceled-By",         HTstd),
    ARTHEADERINIT("Cancel-Key",            HTstd),
    ARTHEADERINIT("User-Agent",            HTstd),
    ARTHEADERINIT("X-Original-Message-ID", HTstd),
    ARTHEADERINIT("Cancel-Lock",           HTstd),
    ARTHEADERINIT("Content-Transfer-Encoding",   HTstd),
    ARTHEADERINIT("Face",                  HTstd),
    ARTHEADERINIT("Injection-Info",        HTstd),
    ARTHEADERINIT("List-ID",               HTstd),
    ARTHEADERINIT("MIME-Version",          HTstd),
    ARTHEADERINIT("Originator",            HTstd),
    ARTHEADERINIT("X-Auth",                HTstd),
    ARTHEADERINIT("X-Complaints-To",       HTstd),
    ARTHEADERINIT("X-Face",                HTstd),
    ARTHEADERINIT("X-HTTP-UserAgent",      HTstd),
    ARTHEADERINIT("X-HTTP-Via",            HTstd),
    ARTHEADERINIT("X-Modbot",              HTstd),
    ARTHEADERINIT("X-Modtrace",            HTstd),
    ARTHEADERINIT("X-No-Archive",          HTstd),
    ARTHEADERINIT("X-Original-Trace",      HTstd),
    ARTHEADERINIT("X-Originating-IP",      HTstd),
    ARTHEADERINIT("X-PGP-Key",             HTstd),
    ARTHEADERINIT("X-PGP-Sig",             HTstd),
    ARTHEADERINIT("X-Poster-Trace",        HTstd),
    ARTHEADERINIT("X-Postfilter",          HTstd),
    ARTHEADERINIT("X-Proxy-User",          HTstd),
    ARTHEADERINIT("X-Submissions-To",      HTstd),
    ARTHEADERINIT("X-Usenet-Provider",     HTstd),
    ARTHEADERINIT("In-Reply-To",           HTstd),
    ARTHEADERINIT("Injection-Date",        HTstd),
    ARTHEADERINIT("NNTP-Posting-Date",     HTstd),
    ARTHEADERINIT("X-User-ID",             HTstd),
    ARTHEADERINIT("X-Auth-Sender",         HTstd),
    ARTHEADERINIT("X-Original-NNTP-Posting-Host", HTstd),
    ARTHEADERINIT("Original-Sender",       HTstd),
    ARTHEADERINIT("NNTP-Posting-Path",     HTstd),
    ARTHEADERINIT("Archive",               HTstd),
    ARTHEADERINIT("Archived-At",           HTstd),
    ARTHEADERINIT("Summary",               HTstd)
};

/* Dummy functions that innd.c provides. */
void CleanupAndExit(int status UNUSED, const char *why UNUSED) { }
void JustCleanup(void) { }
void ReopenLog(FILE *F UNUSED) { }
