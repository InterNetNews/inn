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
    ARTHEADERINIT("X-Original-Message-ID", HTstd)
};

/* Dummy functions that innd.c provides. */
void CleanupAndExit(int status UNUSED, const char *why UNUSED) { }
void JustCleanup(void) { }
void ReopenLog(FILE *F UNUSED) { }
