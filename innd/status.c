
/* $Id$ */

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "clibrary.h"
#include "configdata.h"
#include "innd.h"
#include "logging.h"

#define MAX_PEER     100
#define MIN_REFRESH   60  /* 1 min */
#define HTML_STATUS
#define STATUS_FILE  "inn.status" /* will be in MOST_LOG */

typedef struct _STATUS {
    char           name[SMBUF];
    char           ip_addr[15];
    BOOL           can_stream;
    unsigned short activeCxn;
    unsigned short sleepingCxns;
    unsigned long  seconds;
    unsigned long  accepted;
    unsigned long  refused;
    unsigned long  rejected;
    unsigned long  Duplicate;
    unsigned long  Unwanted_u;
    unsigned long  Unwanted_d;
    unsigned long  Unwanted_g;
    unsigned long  Unwanted_s;
    unsigned long  Unwanted_f;
    unsigned long  Size;
    unsigned long  Check;
    unsigned long  Check_send;
    unsigned long  Check_deferred;
    unsigned long  Check_got;
    unsigned long  Takethis;
    unsigned long  Takethis_Ok;
    unsigned long  Takethis_Err;
    unsigned long  Ihave;
    unsigned long  Ihave_Duplicate;
    unsigned long  Ihave_Deferred;
    unsigned long  Ihave_SendIt;
} STATUS;

static unsigned STATUSlast_time;
char            start_time[50];

static unsigned STATUSgettime(void)
{
  static int	    	init = 0;
  static struct timeval	start_tv;
  struct timeval	tv;
  
  if (!init) {
    gettimeofday(&start_tv, NULL);
    init++;
  }
  gettimeofday(&tv, NULL);
  return((tv.tv_sec - start_tv.tv_sec) * 1000 +
	 (tv.tv_usec - start_tv.tv_usec) / 1000);
}

void STATUSinit(void)
{
  time_t now;
  
  STATUSlast_time = STATUSgettime();	/* First invocation */
  now = time (NULL) ;
  strcpy (start_time, ctime (&now)) ;
}

char *PrettySize (unsigned long size, char *str)
{
  if (size > 1073741824) /* 1024*1024*1024 */
    sprintf (str, "%.1fGb", size / 1073741824.);
  else
    if (size > 1048576) /* 1024*1024 */
      sprintf (str, "%.1fMb", size / 1048576.);
    else
      sprintf (str, "%.1fkb", size / 1024.);
  return (str);
}

static void STATUSsummary(void)
{
  FILE             *F;
  int               i, j;
  register CHANNEL *cp;
  int               activeCxn = 0;
  int               sleepingCxns = 0;
  unsigned long     seconds = 0;
  unsigned long     duplicate = 0;
  unsigned long     offered;
  unsigned long     accepted = 0;
  unsigned long     refused = 0;
  unsigned long     rejected = 0;
  unsigned long     size = 0;
  int               peers = 0;
  char              TempString [SMBUF];
  STATUS            status[MAX_PEER];
  char              str[9];
  time_t            now;
#if defined(DO_PERL)
  extern int        PerlFilterActive;
#endif /* defined(DO_PERL) */
 
  sprintf (TempString, "%s/%s", innconf->pathlog, STATUS_FILE);
  if ((F = fopen(TempString, "w")) == NULL)
    return;

#if defined(HTML_STATUS)
  /* HTML Header */

  fprintf (F,"<HTML>\n<HEAD>\n<META HTTP-EQUIV=\"Refresh\" CONTENT=\"%d;\">\n",
	   innconf->status < MIN_REFRESH ? MIN_REFRESH : innconf->status);
  fprintf (F, "<TITLE>%s: incoming feeds</TITLE>\n", Path.Data);
  fprintf (F, "</HEAD>\n<BODY>\n<PRE>\n") ;
#endif /* defined(HTML_STATUS) */

  fprintf (F, "%s\n", Version);
  fprintf (F, "pid %d started %s\n", (int) getpid(), start_time);

  for (i = 0; (cp = CHANiter(&i, CTnntp)) != NULL; ) {
    j = 0;
    strcpy(TempString, cp->Address.s_addr == 0 ? "localhost" : RChostname(cp));
    while ((j < peers) && (strcmp (TempString, status[j].name)))
      j++;
    if (j > MAX_PEER) {
      syslog(L_NOTICE, "%s: too many peers for STATUSsummary()", TempString);
      fprintf (F, "%s: too many peers for STATUSsummary()\n", TempString);
      continue;
    }
    if (j == peers) {
      peers++;                                              /* a new peer */
      strcpy (status[j].name, TempString);                         /* name */
      strcpy (status[j].ip_addr, inet_ntoa(cp->Address));    /* ip address */
      status[j].can_stream = cp->Streaming;
      status[j].seconds = status[j].Size = 0;
      status[j].Ihave = status[j].Ihave_Duplicate =
	status[j].Ihave_Deferred = status[j].Ihave_SendIt = 0;
      status[j].Check = status[j].Check_send = 
	status[j].Check_deferred = status[j].Check_got = 0;
      status[j].Takethis = status[j].Takethis_Ok = status[j].Takethis_Err = 0;
      status[j].activeCxn = status[j].sleepingCxns = 0;
      status[j].accepted = 0;
      status[j].refused = status[j].rejected = 0;
      status[j].Duplicate = status[j].Unwanted_u = 0;
      status[j].Unwanted_d = status[j].Unwanted_g = 0;
      status[j].Unwanted_s = status[j].Unwanted_f = 0;
    }
    if (Now.time - cp->Started > status[j].seconds)
      status[j].seconds = Now.time - cp->Started;
    if (Now.time - cp->Started > seconds)
      seconds = Now.time - cp->Started;
    status[j].accepted += cp->Received;
    accepted += cp->Received;
    status[j].refused += cp->Refused;
    refused += cp->Refused;
    status[j].rejected += cp->Rejected;
    rejected += cp->Rejected;
    status[j].Duplicate += cp->Duplicate;
    duplicate += cp->Duplicate;
    status[j].Unwanted_u += cp->Unwanted_u;
    status[j].Unwanted_d += cp->Unwanted_d;
    status[j].Unwanted_g += cp->Unwanted_g;
    status[j].Unwanted_s += cp->Unwanted_s;
    status[j].Unwanted_f += cp->Unwanted_f;
    status[j].Ihave += cp->Ihave;
    status[j].Ihave_Duplicate += cp->Ihave_Duplicate;
    status[j].Ihave_Deferred += cp->Ihave_Deferred;
    status[j].Ihave_SendIt += cp->Ihave_SendIt;
    status[j].Check += cp->Check;
    status[j].Check_send += cp->Check_send;
    status[j].Check_deferred += cp->Check_deferred;
    status[j].Check_got += cp->Check_got;
    status[j].Takethis += cp->Takethis;
    status[j].Takethis_Ok += cp->Takethis_Ok;
    status[j].Takethis_Err += cp->Takethis_Err;
    status[j].Size += cp->Size;
    size += cp->Size;
    if (CHANsleeping(cp)) {
      sleepingCxns++;
      status[j].sleepingCxns++;
    } else {
      activeCxn++;
      status[j].activeCxn++;
    }
  }

  /* Header */
  now = time (NULL);
  strcpy (TempString, ctime (&now));
  fprintf (F, "Updated: %s", TempString);
  fprintf (F, "(peers: %d, active-cxns: %d, sleeping-cxns: %d)\n\n",
	   peers, activeCxn, sleepingCxns);

  fprintf (F, "Mode: %s", Mode == OMrunning ? "running" :
	   Mode == OMpaused ? "paused" :
	   Mode == OMthrottled ? "throttled" : "Unknown");
  if ((Mode == OMpaused) || (Mode == OMthrottled))
    fprintf (F, " (%s)", ModeReason);
  
  /* Global configuration */
  fprintf (F, "\n\nConfiguration file: %s\n\n", _PATH_CONFIG);

  fprintf (F, "Global configuration parameters:\n");
  fprintf (F, "              Largest Article: %ld bytes\n", innconf->maxartsize);
  fprintf (F, "     Max Incoming connections: ");
  if (innconf->maxconnections)
    fprintf (F, "%d\n", innconf->maxconnections);
  else
    fprintf (F, "unlimited\n");
  fprintf (F, "      Max Outgoing file feeds: %d\n", MaxOutgoing);
  fprintf (F, "                       Cutoff: ");
  if (innconf->artcutoff)
    fprintf (F, "%d days\n", (int) (innconf->artcutoff / 3600L / 24L));
  else
    fprintf (F, "none\n");
  fprintf (F, "               Timeout period: %ld seconds\n",
	   (long)TimeOut.tv_sec);
  if (innconf->remembertrash) {
	fprintf (F, "               Remember Trash: Yes\n");
  } else {
	fprintf (F, "               Remember Trash: No\n");
  }
#if defined(DO_TCL)
  fprintf (F, "                Tcl filtering: %s\n", 
	   TCLFilterActive ? "enabled" : "disabled");
#endif /* defined(DO_TCL) */
#if defined(DO_PERL)
  fprintf (F, "               Perl filtering: %s\n", 
	   PerlFilterActive ? "enabled" : "disabled");
#endif /* defined(DO_PERL) */

  fputc ('\n', F) ;

  /* Global values */
  fprintf (F, "global (process)\n");
  fprintf (F, "    seconds: %ld\n", seconds);
  offered = accepted + refused + rejected;
  fprintf (F, "    offered: %-9ld\n", offered);
  if (!offered) offered = 1; /* to avoid division by zero */
  fprintf (F, "   accepted: %-9ld  %%accepted: %.1f%%\n",
	   accepted, (float) accepted / offered * 100);
  fprintf (F, "    refused: %-9ld   %%refused: %.1f%%\n",
	   refused, (float) refused / offered * 100);
  fprintf (F, "   rejected: %-9ld  %%rejected: %.1f%%\n",
	   rejected, (float) rejected / offered * 100);
  fprintf (F, "      bytes: %-7s\n", PrettySize (size, str));
  fputc ('\n', F) ;
  
  /* Incoming Feeds */
  for (j = 0; j < peers; j++) {
    fprintf (F, "%s\n",                      status[j].name);
    fprintf (F, "    seconds: %-7ld  ",      status[j].seconds);
    fprintf (F, "      duplicates: %-7ld ",  status[j].Duplicate);
    fprintf (F, "    ip address: %s\n",      status[j].ip_addr);
    fprintf (F, "    offered: %-7ld  ",
	     status[j].accepted + status[j].refused + status[j].rejected);
    fprintf (F, "   uw newsgroups: %-7ld ",  status[j].Unwanted_g);
    fprintf (F, "   active cxns: %d\n",      status[j].activeCxn);
    fprintf (F, "   accepted: %-7ld  ",      status[j].accepted);
    fprintf (F, "uw distributions: %-7ld ",  status[j].Unwanted_d);
    fprintf (F, " sleeping cxns: %d\n",      status[j].sleepingCxns);
    fprintf (F, "    refused: %-7ld  ",      status[j].refused);
    fprintf (F, "      unapproved: %-7ld ",  status[j].Unwanted_u);
    fprintf (F, "want streaming: %s\n",
	     status[j].can_stream ? "Yes" : "No");
    fprintf (F, "   rejected: %-7ld  ",      status[j].rejected);
    fprintf (F, "         filtred: %-7ld ",  status[j].Unwanted_f);
    fprintf (F, "  is streaming: %s\n",
	     (status[j].Check || status[j].Takethis) ? "Yes" : "No");
    fprintf (F, "       size: %-8s ",        PrettySize(status[j].Size, str));
    fprintf (F, "       bad sites: %-7ld\n", status[j].Unwanted_s);
    fprintf (F, "  Protocol:\n");
    fprintf (F, "      Ihave: %-6ld SendIt[%d]: %-6ld    Got[%d]: %-6ld Deferred[%d]: %ld\n",
	     status[j].Ihave, NNTP_SENDIT_VAL, status[j].Ihave_SendIt,
	     NNTP_HAVEIT_VAL, status[j].Ihave_Duplicate, NNTP_RESENDIT_VAL,
	     status[j].Ihave_Deferred);
    fprintf (F, "      Check: %-6ld SendIt[%d]: %-6ld    Got[%d]: %-6ld Deferred[%d]: %ld\n",
	     status[j].Check, NNTP_OK_SENDID_VAL, status[j].Check_send,
	     NNTP_ERR_GOTID_VAL, status[j].Check_got, NNTP_RESENDID_VAL,
	     status[j].Check_deferred);
    fprintf (F, "   Takethis: %-6ld     Ok[%d]: %-6ld  Error[%d]: %-6ld\n",
	     status[j].Takethis, NNTP_OK_RECID_VAL, status[j].Takethis_Ok,
	     NNTP_ERR_FAILID_VAL, status[j].Takethis_Err);

    fputc ('\n', F) ;
  }

#if defined(HTML_STATUS)
  /* HTML Footer */
  fprintf (F,"</PRE>\n</BODY>\n</HTML>\n");
#endif /* defined(HTML_STATUS) */

  (void)fclose(F);
}

void STATUSmainloophook(void)
{
  unsigned now;
    
  if (!innconf->status)
    return;
  now = STATUSgettime();
  
  if (now - STATUSlast_time > (innconf->status * 1000)) {
    STATUSsummary();
    STATUSlast_time = now;
  }
}

