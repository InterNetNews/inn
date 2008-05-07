/*  $Id$
**
**  inn.conf parser interface.
**
**  The interface to reading inn.conf configuration files and managing the
**  resulting innconf struct.
*/

#ifndef INN_INNCONF_H
#define INN_INNCONF_H 1

#include <inn/defines.h>
#include <stdio.h>

/*
**  This structure is organized in the same order as the variables contained
**  in it are mentioned in the inn.conf documentation, and broken down into
**  the same sections.  Note that due to the implementation, only three types
**  of variables are permissible here:  char *, bool, and long.
*/
struct innconf {
    /* General Settings */
    char *domain;               /* Default domain of local host */
    char *innflags;             /* Flags to pass to innd on startup */
    char *mailcmd;              /* Command to send report/control type mail */
    char *mta;                  /* MTA for mailing to moderators, innmail */
    char *pathhost;             /* Entry for the Path: line */
    char *server;               /* Default server to connect to */
    char *runasuser;            /* User to run under */
    char *runasgroup;           /* Group to run under */

    /* Feed Configuration */
    long artcutoff;             /* Max accepted article age */
    char *bindaddress;          /* Which interface IP to bind to */
    char *bindaddress6;         /* Which interface IPv6 to bind to */
    bool dontrejectfiltered;    /* Don't reject filtered article? */
    long hiscachesize;          /* Size of the history cache in kB */
    bool ignorenewsgroups;      /* Propagate cmsgs by affected group? */
    bool immediatecancel;       /* Immediately cancel timecaf messages? */
    long linecountfuzz;         /* Check linecount and reject if off by more */
    long maxartsize;            /* Reject articles bigger than this */
    long maxconnections;        /* Max number of incoming NNTP connections */
    char *pathalias;            /* Prepended Host for the Path: line */
    char *pathcluster;          /* Appended Host for the Path: line */
    bool pgpverify;             /* Verify control messages with pgpverify? */
    long port;                  /* Which port innd should listen on */
    bool refusecybercancels;    /* Reject message IDs with "<cancel."? */
    bool remembertrash;         /* Put unwanted article IDs into history */
    char *sourceaddress;        /* Source IP for outgoing NNTP connections */
    char *sourceaddress6;       /* Source IPv6 for outgoing NNTP connections */
    bool verifycancels;         /* Verify cancels against article author */
    bool wanttrash;             /* Put unwanted articles in junk */
    long wipcheck;              /* How long to defer other copies of article */
    long wipexpire;             /* How long to keep pending article record */

    /* History settings */
    char *hismethod;            /* Which history method to use */
    
    /* Article Storage */
    long cnfscheckfudgesize;    /* Additional CNFS integrity checking */
    bool enableoverview;        /* Store overview info for articles? */
    bool groupbaseexpiry;       /* Do expiry by newsgroup? */
    bool mergetogroups;         /* Refile articles from to.* into to */
    bool nfswriter;             /* Use NFS writer functionality */
    long overcachesize;         /* fd size cache for tradindexed */
    char *ovgrouppat;           /* Newsgroups to store overview for */
    char *ovmethod;             /* Which overview method to use */
    bool storeonxref;           /* SMstore use Xref to detemine class? */
    bool useoverchan;           /* overchan write the overview, not innd? */
    bool wireformat;            /* Store tradspool articles in wire format? */
    bool xrefslave;             /* Act as a slave of another server? */

    /* Reading */
    bool allownewnews;          /* Allow use of the NEWNEWS command */
    bool articlemmap;           /* Use mmap to read articles? */
    long clienttimeout;         /* How long nnrpd can be inactive */
    long initialtimeout;        /* How long nnrpd waits for first command */
    long msgidcachesize;        /* Number of entries in the message ID cache */
    bool nfsreader;             /* Use NFS reader functionality */
    long nfsreaderdelay;        /* Delay applied to article arrival */
    bool nnrpdcheckart;         /* Check article existence before returning? */
    char *nnrpdflags;           /* Arguments to pass when spawning nnrpd */
    long nnrpdloadlimit;        /* Maximum getloadvg() we allow */
    bool noreader;              /* Refuse to fork nnrpd for readers? */
    bool readerswhenstopped;    /* Allow nnrpd when server is paused */
    bool readertrack;           /* Use the reader tracking system? */
    bool tradindexedmmap;       /* Whether to mmap for tradindexed */

    /* Reading -- Keyword Support */
    bool keywords;              /* Generate keywords in overview? */
    long keyartlimit;           /* Max article size for keyword generation */
    long keylimit;              /* Max allocated space for keywords */
    long keymaxwords;           /* Max count of interesting words */

    /* Posting */
    bool addnntppostingdate;    /* Add NNTP-Posting-Date: to posts */
    bool addnntppostinghost;    /* Add NNTP-Posting-Host: to posts */
    bool checkincludedtext;     /* Reject if too much included text */
    char *complaints;           /* Address for X-Complaints-To: */
    char *fromhost;             /* Host for the From: line */
    long localmaxartsize;       /* Max article size of local postings */
    char *moderatormailer;      /* Default host to mail moderated articles */
    bool nnrpdauthsender;       /* Add authenticated Sender: header? */
    char *nnrpdposthost;        /* Host postings should be forwarded to */
    long nnrpdpostport;         /* Port postings should be forwarded to */
    char *organization;         /* Data for the Organization: header */
    bool spoolfirst;            /* Spool all posted articles? */
    bool strippostcc;           /* Strip To:, Cc: and Bcc: from posts */

    /* Posting -- Exponential Backoff */
    bool backoffauth;           /* Backoff by user, not IP address */
    char *backoffdb;            /* Directory for backoff databases */
    long backoffk;              /* Multiple for the sleep time */
    long backoffpostfast;       /* Upper time limit for fast posting */
    long backoffpostslow;       /* Lower time limit for slow posting */
    long backofftrigger;        /* Number of postings before triggered */

    /* Reading and posting -- SSL and TLS support */
#ifdef HAVE_SSL
    char *tlscafile;            /* Path to a certificate authority file */
    char *tlscapath;            /* Path to a directory of CA certificates */
    char *tlscertfile;          /* Path to the SSL certificate to use */
    char *tlskeyfile;           /* Path to the key for the certificate */
#endif /* HAVE_SSL */

    /* Monitoring */
    bool doinnwatch;            /* Start innwatch from rc.news? */
    long innwatchbatchspace;    /* Minimum free space in pathoutgoing */
    long innwatchlibspace;      /* Minimum free space in pathdb */
    long innwatchloload;        /* Load times 100 at which to restart */
    long innwatchhiload;        /* Load times 100 at which to throttle */
    long innwatchpauseload;     /* Load times 100 at which to pause */
    long innwatchsleeptime;     /* Seconds to wait between checks */
    long innwatchspoolnodes;    /* Minimum free inodes in patharticles */
    long innwatchspoolspace;    /* Minimum free space in patharticles */

    /* Logging */
    bool docnfsstat;            /* Run cnfsstat in the background? */
    long incominglogfrequency;  /* Checkpoint log after this many articles */
    bool logartsize;            /* Log article sizes? */
    bool logcancelcomm;         /* Log ctlinnd cancel commands to syslog? */
    long logcycles;             /* How many old logs scanlogs should keep */
    bool logipaddr;             /* Log by host IP address? */
    bool logsitename;           /* Log outgoing site names? */
    bool logstatus;             /* Send a status report to syslog? */
    bool nnrpdoverstats;        /* Log overview statistics? */
    bool nntplinklog;           /* Put storage token into the log? */
    long status;                /* Status file update interval */
    char *stathist;             /* Filename for history profiler outputs */
    long timer;                 /* Performance monitoring interval */

    /* System Tuning */
    long badiocount;            /* Failure count before dropping channel */
    long blockbackoff;          /* Multiplier for sleep in EAGAIN writes */
    long chaninacttime;         /* Wait before noticing inactive channels */
    long chanretrytime;         /* How long before channel restarts */
    long datamovethreshold;     /* Threshold to extend buffer or move data */
    long icdsynccount;          /* Articles between active & history updates */
    long keepmmappedthreshold;  /* Threshold for keeping mmap in buffindexed */
    long maxcmdreadsize;        /* Max NNTP command read size used by innd */
    long maxforks;              /* Give up after this many fork failure */
    long nicekids;              /* Child processes get niced to this */
    long nicenewnews;           /* If NEWNEWS command is used, nice to this */
    long nicennrpd;             /* nnrpd is niced to this */
    long pauseretrytime;        /* Seconds before seeing if pause is ended */
    long peertimeout;           /* How long peers can be inactive */
    long rlimitnofile;          /* File descriptor limit to set */

    /* Paths */
    char *patharchive;          /* Archived news */
    char *patharticles;         /* Articles */
    char *pathbin;              /* News binaries */
    char *pathcontrol;          /* Path to control message handlers */
    char *pathdb;               /* News database files */
    char *pathetc;              /* News configuration files */
    char *pathfilter;           /* Filtering code */
    char *pathhttp;             /* HTML files */
    char *pathincoming;         /* Incoming spooled news */
    char *pathlog;              /* Log files */
    char *pathnews;             /* Home directory for news user */
    char *pathoutgoing;         /* Outgoing news batch files */
    char *pathoverview;         /* Overview infomation */
    char *pathrun;              /* Runtime state and sockets */
    char *pathspool;            /* Root of news spool hierarchy */
    char *pathtmp;              /* Temporary files for the news system */
};

/* The global innconf variable used in programs. */
extern struct innconf *innconf;

/* Used to request various types of quoting when printing out values. */
enum innconf_quoting {
    INNCONF_QUOTE_NONE,
    INNCONF_QUOTE_SHELL,
    INNCONF_QUOTE_PERL,
    INNCONF_QUOTE_TCL
};

BEGIN_DECLS

/* Parse the given file into innconf, using the default path if NULL. */
bool innconf_read(const char *path);

/* Free an innconf struct and all allocated memory for it. */
void innconf_free(struct innconf *);

/* Print a single value with appropriate quoting, return whether found. */
bool innconf_print_value(FILE *, const char *key, enum innconf_quoting);

/* Dump the entire configuration with appropriate quoting. */
void innconf_dump(FILE *, enum innconf_quoting);

/* Compare two instances of an innconf struct, for testing. */
bool innconf_compare(struct innconf *, struct innconf *);

/* Check the validity of an inn.conf file.  Does innconf_read plus checking
   for any unknown parameters that are set. */
bool innconf_check(const char *path);

END_DECLS

#endif /* INN_INNCONF_H */
