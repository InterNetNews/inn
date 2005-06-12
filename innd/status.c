/*  $Id$
**
**  Periodic status reporting.
*/
#include "config.h"
#include "clibrary.h"
#include "portable/socket.h"

#include "inn/network.h"
#include "inn/innconf.h"
#include "inn/version.h"
#include "innd.h"
#include "innperl.h"

#define MIN_REFRESH   60  /* 1 min */
#define HTML_STATUS
#if defined(HTML_STATUS)
#define STATUS_FILE	"inn_status.html"	/* will be in pathhttp */
#else
#define STATUS_FILE	"inn.status"		/* will be in pathlog */
#endif

typedef struct _STATUS {
    char		name[SMBUF];
    char		ip_addr[INET6_ADDRSTRLEN];
    bool		can_stream;
    unsigned short	activeCxn;
    unsigned short	sleepingCxns;
    time_t		seconds;
    unsigned long	accepted;
    unsigned long	refused;
    unsigned long	rejected;
    unsigned long	Duplicate;
    unsigned long	Unwanted_u;
    unsigned long	Unwanted_d;
    unsigned long	Unwanted_g;
    unsigned long	Unwanted_s;
    unsigned long	Unwanted_f;
    float		Size;
    float		DuplicateSize;
    float		RejectSize;
    unsigned long	Check;
    unsigned long	Check_send;
    unsigned long	Check_deferred;
    unsigned long	Check_got;
    unsigned long	Check_cybercan;
    unsigned long	Takethis;
    unsigned long	Takethis_Ok;
    unsigned long	Takethis_Err;
    unsigned long	Ihave;
    unsigned long	Ihave_Duplicate;
    unsigned long	Ihave_Deferred;
    unsigned long	Ihave_SendIt;
    unsigned long	Ihave_Cybercan;
    struct _STATUS *next;
} STATUS;

static unsigned STATUSlast_time;
char            start_time[50];

static unsigned
STATUSgettime(void)
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

void
STATUSinit(void)
{
  time_t now;
  
  STATUSlast_time = STATUSgettime();	/* First invocation */
  now = time (NULL) ;
  strlcpy(start_time, ctime(&now), sizeof(start_time));
}

static char *
PrettySize(float size, char *str)
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

static void
STATUSsummary(void)
{
  FILE			*F;
  int			i, j;
  CHANNEL               *cp;
  int			activeCxn = 0;
  int			sleepingCxns = 0;
  time_t		seconds = 0;
  unsigned long		duplicate = 0;
  unsigned long		offered;
  unsigned long		accepted = 0;
  unsigned long		refused = 0;
  unsigned long		rejected = 0;
  float			size = 0;
  float			DuplicateSize = 0;
  float			RejectSize = 0;
  int			peers = 0;
  char                  TempString[SMBUF];
  char			*path;
  STATUS		*head, *status, *tmp;
  char			str[9];
  time_t		now;
 
#if defined(HTML_STATUS)
  path = concatpath(innconf->pathhttp, STATUS_FILE);
#else
  path = concatpath(innconf->pathlog, STATUS_FILE);
#endif
  if ((F = Fopen(path, "w", TEMPORARYOPEN)) == NULL)
    return;

#if defined(HTML_STATUS)
  /* HTML Header */

  fprintf (F,"<HTML>\n<HEAD>\n<META HTTP-EQUIV=\"Refresh\" CONTENT=\"%ld;\">\n",
	   innconf->status < MIN_REFRESH ? MIN_REFRESH : innconf->status);
  fprintf (F, "<TITLE>%s: incoming feeds</TITLE>\n", innconf->pathhost);
  fprintf (F, "</HEAD>\n<BODY>\n<PRE>\n") ;
#endif /* defined(HTML_STATUS) */

  fprintf (F, "%s\n", INN_VERSION_STRING);
  fprintf (F, "pid %d started %s\n", (int) getpid(), start_time);

  tmp = head = NULL;
  for (i = 0; (cp = CHANiter(&i, CTnntp)) != NULL; ) {
    j = 0;
    strlcpy(TempString,
	    cp->Address.ss_family == 0 ? "localhost" : RChostname(cp),
            sizeof(TempString));
    for (status = head ; status != NULL ; status = status->next) {
	if (strcmp(TempString, status->name) == 0)
	    break;
    }
    if (status == NULL) {
      status = xmalloc(sizeof(STATUS));
      peers++;                                              /* a new peer */
      strlcpy(status->name, TempString, sizeof(status->name));
      network_sockaddr_sprint(status->ip_addr, sizeof(status->ip_addr),
                              (struct sockaddr *) &cp->Address);
      status->can_stream = cp->Streaming;
      status->seconds = status->Size = status->DuplicateSize = 0;
      status->RejectSize = 0;
      status->Ihave = status->Ihave_Duplicate =
	status->Ihave_Deferred = status->Ihave_SendIt =
	status->Ihave_Cybercan = 0;
      status->Check = status->Check_send = 
	status->Check_deferred = status->Check_got =
	status->Check_cybercan = 0;
      status->Takethis = status->Takethis_Ok = status->Takethis_Err = 0;
      status->activeCxn = status->sleepingCxns = 0;
      status->accepted = 0;
      status->refused = status->rejected = 0;
      status->Duplicate = status->Unwanted_u = 0;
      status->Unwanted_d = status->Unwanted_g = 0;
      status->Unwanted_s = status->Unwanted_f = 0;
      status->next = NULL;
      if (head == NULL)
	head = status;
      else
	tmp->next = status;
      tmp = status;
    }
    if (Now.tv_sec - cp->Started > status->seconds)
      status->seconds = Now.tv_sec - cp->Started;
    if (Now.tv_sec - cp->Started > seconds)
      seconds = Now.tv_sec - cp->Started;
    status->accepted += cp->Received;
    accepted += cp->Received;
    status->refused += cp->Refused;
    refused += cp->Refused;
    status->rejected += cp->Rejected;
    rejected += cp->Rejected;
    status->Duplicate += cp->Duplicate;
    duplicate += cp->Duplicate;
    status->Unwanted_u += cp->Unwanted_u;
    status->Unwanted_d += cp->Unwanted_d;
    status->Unwanted_g += cp->Unwanted_g;
    status->Unwanted_s += cp->Unwanted_s;
    status->Unwanted_f += cp->Unwanted_f;
    status->Ihave += cp->Ihave;
    status->Ihave_Duplicate += cp->Ihave_Duplicate;
    status->Ihave_Deferred += cp->Ihave_Deferred;
    status->Ihave_SendIt += cp->Ihave_SendIt;
    status->Ihave_Cybercan += cp->Ihave_Cybercan;
    status->Check += cp->Check;
    status->Check_send += cp->Check_send;
    status->Check_deferred += cp->Check_deferred;
    status->Check_got += cp->Check_got;
    status->Check_cybercan += cp->Check_cybercan;
    status->Takethis += cp->Takethis;
    status->Takethis_Ok += cp->Takethis_Ok;
    status->Takethis_Err += cp->Takethis_Err;
    status->Size += cp->Size;
    status->DuplicateSize += cp->DuplicateSize;
    status->RejectSize += cp->RejectSize;
    size += cp->Size;
    DuplicateSize += cp->DuplicateSize;
    RejectSize += cp->RejectSize;
    if (CHANsleeping(cp)) {
      sleepingCxns++;
      status->sleepingCxns++;
    } else {
      activeCxn++;
      status->activeCxn++;
    }
  }

  /* Header */
  now = time (NULL);
  strlcpy (TempString, ctime (&now), sizeof(TempString));
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
    fprintf (F, "%ld\n", innconf->maxconnections);
  else
    fprintf (F, "unlimited\n");
  fprintf (F, "      Max Outgoing file feeds: %d\n", MaxOutgoing);
  fprintf (F, "                       Cutoff: ");
  if (innconf->artcutoff)
    fprintf (F, "%ld days\n", innconf->artcutoff);
  else
    fprintf (F, "none\n");
  fprintf (F, "               Timeout period: %ld seconds\n",
	   (long)TimeOut.tv_sec);
  if (innconf->remembertrash) {
	fprintf (F, "               Remember Trash: Yes\n");
  } else {
	fprintf (F, "               Remember Trash: No\n");
  }
#ifdef DO_PERL
  fprintf (F, "               Perl filtering: %s\n", 
	   PerlFilterActive ? "enabled" : "disabled");
#endif

  fputc ('\n', F) ;

  /* Global values */
  fprintf (F, "global (process)\n");
  fprintf (F, "         seconds: %ld\n", (long) seconds);
  offered = accepted + refused + rejected;
  fprintf (F, "         offered: %-9ld\n", offered);
  if (!offered) offered = 1; /* to avoid division by zero */
  if (!size) size = 1; /* avoid divide by zero here too */
  fprintf (F, "        accepted: %-9ld       %%accepted: %.1f%%\n",
	   accepted, (float) accepted / offered * 100);
  fprintf (F, "         refused: %-9ld        %%refused: %.1f%%\n",
	   refused, (float) refused / offered * 100);
  fprintf (F, "        rejected: %-9ld       %%rejected: %.1f%%\n",
	   rejected, (float) rejected / offered * 100);
  fprintf (F, "      duplicated: %-9ld     %%duplicated: %.1f%%\n",
	   duplicate, (float) duplicate / offered * 100);
  fprintf (F, "           bytes: %-7s\n", PrettySize (size + DuplicateSize + RejectSize, str));
  fprintf (F, " duplicated size: %-7s  %%duplicated size: %.1f%%\n",
	   PrettySize(DuplicateSize, str), (float) DuplicateSize / size * 100);
  fprintf (F, "   rejected size: %-7s    %%rejected size: %.1f%%\n",
	   PrettySize(RejectSize, str), (float) RejectSize / size * 100);
  fputc ('\n', F) ;
  
  /* Incoming Feeds */
  for (status = head ; status != NULL ;) {
    fprintf (F, "%s\n",                      status->name);
    fprintf (F, "    seconds: %-7ld  ",      (long) status->seconds);
    fprintf (F, "      duplicates: %-7ld ",  status->Duplicate);
    fprintf (F, "    ip address: %s\n",      status->ip_addr);
    fprintf (F, "    offered: %-7ld  ",
	     status->accepted + status->refused + status->rejected);
    fprintf (F, "   uw newsgroups: %-7ld ",  status->Unwanted_g);
    fprintf (F, "   active cxns: %d\n",      status->activeCxn);
    fprintf (F, "   accepted: %-7ld  ",      status->accepted);
    fprintf (F, "uw distributions: %-7ld ",  status->Unwanted_d);
    fprintf (F, " sleeping cxns: %d\n",      status->sleepingCxns);
    fprintf (F, "    refused: %-7ld  ",      status->refused);
    fprintf (F, "      unapproved: %-7ld ",  status->Unwanted_u);
    fprintf (F, "want streaming: %s\n",
	     status->can_stream ? "Yes" : "No");
    fprintf (F, "   rejected: %-7ld  ",      status->rejected);
    fprintf (F, "        filtered: %-7ld ",  status->Unwanted_f);
    fprintf (F, "  is streaming: %s\n",
	     (status->Check || status->Takethis) ? "Yes" : "No");
    fprintf (F, "       size: %-8s ",        PrettySize(status->Size, str));
    fprintf (F, "       bad sites: %-7ld ", status->Unwanted_s);
    fprintf (F, "duplicate size: %s\n", PrettySize(status->DuplicateSize, str));
    fprintf (F, "reject size: %-8s\n",       PrettySize(status->RejectSize, str));
    fprintf (F, "  Protocol:\n");
    fprintf (F, "      Ihave: %-6ld SendIt[%d]: %-6ld    Got[%d]: %-6ld Deferred[%d]: %ld\n",
	     status->Ihave, NNTP_SENDIT_VAL, status->Ihave_SendIt,
	     NNTP_HAVEIT_VAL, status->Ihave_Duplicate, NNTP_RESENDIT_VAL,
	     status->Ihave_Deferred);
    fprintf (F, "      Check: %-6ld SendIt[%d]: %-6ld    Got[%d]: %-6ld Deferred[%d]: %ld\n",
	     status->Check, NNTP_OK_SENDID_VAL, status->Check_send,
	     NNTP_ERR_GOTID_VAL, status->Check_got, NNTP_RESENDID_VAL,
	     status->Check_deferred);
    fprintf (F, "   Takethis: %-6ld     Ok[%d]: %-6ld  Error[%d]: %-6ld\n",
	     status->Takethis, NNTP_OK_RECID_VAL, status->Takethis_Ok,
	     NNTP_ERR_FAILID_VAL, status->Takethis_Err);
    if (innconf->refusecybercancels) {
        fprintf (F, "   Cancelrejects:    Ihave[%d]: %-6ld  Check[%d]: %-6ld\n",
	     NNTP_HAVEIT_VAL, status->Ihave_Cybercan,
	     NNTP_ERR_GOTID_VAL, status->Check_cybercan);
    }
    fputc ('\n', F) ;
    tmp = status->next;
    free(status);
    status = tmp;
  }

#if defined(HTML_STATUS)
  /* HTML Footer */
  fprintf (F,"</PRE>\n</BODY>\n</HTML>\n");
#endif /* defined(HTML_STATUS) */

  Fclose(F);
}

void
STATUSmainloophook(void)
{
  unsigned now;
    
  if (!innconf->status)
    return;
  now = STATUSgettime();
  
  if (now - STATUSlast_time > (unsigned)(innconf->status * 1000)) {
    STATUSsummary();
    STATUSlast_time = now;
  }
}
