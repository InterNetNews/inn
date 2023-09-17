/*
**  inn.conf parser interface.
**
**  The interface to reading inn.conf configuration files and managing the
**  resulting innconf struct.
*/

#ifndef INN_INNCONF_H
#define INN_INNCONF_H 1

#include "inn/macros.h"
#include "inn/portable-stdbool.h"
#include <stdio.h>

#ifndef INN_CONFPARSE_QUOTING
#    define INN_CONFPARSE_QUOTING 1
enum confparse_quoting {
    CONFPARSE_QUOTE_NONE,
    CONFPARSE_QUOTE_SHELL,
    CONFPARSE_QUOTE_PERL,
    CONFPARSE_QUOTE_TCL
};
#endif

/*
**  This structure is organized in the same order as the variables contained
**  in it are mentioned in the inn.conf documentation, and broken down into
**  the same sections.  Note that due to the implementation, only five types
**  of variables are permissible here: char *, bool, struct vector *, long
**  and unsigned long.
*/
struct innconf {
    /* General Settings */
    char *domain;                /* Default domain of local host */
    char *innflags;              /* Flags to pass to innd on startup */
    char *mailcmd;               /* Command to send reports, innmail */
    char *mta;                   /* MTA for mails sent by INN programs */
    char *pathhost;              /* Entry for the Path header field body */
    char *runasuser;             /* User to run under */
    char *runasgroup;            /* Group to run under */
    char *server;                /* Default server to connect to */
    struct vector *syntaxchecks; /* List of syntax checks to perform or not */

    /* Feed Configuration */
    unsigned long artcutoff;    /* Max accepted article age */
    char *bindaddress;          /* Which interface IP to bind to */
    char *bindaddress6;         /* Which interface IPv6 to bind to */
    char *docancels;            /* Which cancels to process */
    bool dontrejectfiltered;    /* Don't reject filtered article? */
    unsigned long hiscachesize; /* Size of the history cache in kB */
    bool ignorenewsgroups;      /* Propagate cmsgs by affected group? */
    bool immediatecancel;       /* Immediately cancel timecaf messages? */
    unsigned long
        linecountfuzz;        /* Check linecount and reject if off by more */
    unsigned long maxartsize; /* Reject articles bigger than this */
    unsigned long maxconnections; /* Max number of incoming NNTP connections */
    char *pathalias;         /* Prepended Host for the Path header field */
    char *pathcluster;       /* Appended Host for the Path header field */
    bool pgpverify;          /* Verify control messages with pgpverify? */
    unsigned long port;      /* Which port innd should listen on */
    bool remembertrash;      /* Put unwanted article IDs into history */
    char *sourceaddress;     /* Source IP for outgoing NNTP connections */
    char *sourceaddress6;    /* Source IPv6 for outgoing NNTP connections */
    bool verifygroups;       /* Check if all newsgroups exist */
    bool wanttrash;          /* Put unwanted articles in junk */
    unsigned long wipcheck;  /* How long to defer other copies of article */
    unsigned long wipexpire; /* How long to keep pending article record */

    /* History settings */
    char *hismethod; /* Which history method to use */

    /* Article Storage */
    unsigned long cnfscheckfudgesize; /* Additional CNFS integrity checking */
    bool enableoverview;              /* Store overview info for articles? */
    struct vector *extraoverviewadvertised; /* Extra overview fields for LIST
                                               OVERVIEW.FMT */
    struct vector
        *extraoverviewhidden;    /* Extra overview fields silently generated */
    bool groupbaseexpiry;        /* Do expiry by newsgroup? */
    bool mergetogroups;          /* Refile articles from to.* into to */
    bool nfswriter;              /* Use NFS writer functionality */
    unsigned long overcachesize; /* fd size cache for tradindexed */
    unsigned long ovflushcount;  /* Articles between buffindexed flushes */
    char *ovgrouppat;            /* Newsgroups to store overview for */
    char *ovmethod;              /* Which overview method to use */
    bool storeonxref;            /* SMstore use Xref to detemine class? */
    bool useoverchan;            /* overchan write the overview, not innd? */
    bool wireformat;             /* Store tradspool articles in wire format? */
    bool xrefslave;              /* Act as a slave of another server? */

    /* Reading */
    bool allownewnews;            /* Allow use of the NEWNEWS command */
    bool articlemmap;             /* Use mmap to read articles? */
    unsigned long clienttimeout;  /* How long nnrpd can be inactive */
    unsigned long initialtimeout; /* How long nnrpd waits for first command */
    unsigned long
        msgidcachesize; /* Number of entries in the message ID cache */
    bool nfsreader;     /* Use NFS reader functionality */
    unsigned long nfsreaderdelay; /* Delay applied to article arrival */
    bool nnrpdcheckart; /* Check article existence before returning? */
    char *nnrpdflags;   /* Arguments to pass when spawning nnrpd */
    unsigned long nnrpdloadlimit; /* Maximum getloadvg() we allow */
    bool noreader;                /* Refuse to fork nnrpd for readers? */
    bool readerswhenstopped;      /* Allow nnrpd when server is paused */
    bool readertrack;             /* Use the reader tracking system? */
    bool tradindexedmmap;         /* Whether to mmap for tradindexed */

    /* Reading -- Keyword Support */
    bool keywords;             /* Generate keywords in overview? */
    unsigned long keyartlimit; /* Max article size for keyword generation */
    unsigned long keylimit;    /* Max allocated space for keywords */
    unsigned long keymaxwords; /* Max count of interesting words */

    /* Posting */
    bool addinjectiondate; /* Add Injection-Date header fields to posts */
    bool
        addinjectionpostingaccount; /* Add posting-account attribute to
                                       Injection-Info header fields in posts */
    bool addinjectionpostinghost;   /* Add posting-host attribute to
                                       Injection-Info header fields in posts */
    bool checkincludedtext;         /* Reject if too much included text */
    char *complaints; /* Address for mail-complaints-to attribute to
                         Injection-Info header fields */
    char *fromhost;   /* Host for the From header field body */
    unsigned long localmaxartsize; /* Max article size of local postings */
    char *moderatormailer;       /* Default host to mail moderated articles */
    bool nnrpdauthsender;        /* Add authenticated Sender header field? */
    char *nnrpdposthost;         /* Host postings should be forwarded to */
    unsigned long nnrpdpostport; /* Port postings should be forwarded to */
    char *organization;          /* Data for the Organization header field */
    bool spoolfirst;             /* Spool all posted articles? */
    bool strippostcc; /* Strip To, Cc and Bcc header fields from posts */

    /* Posting -- Exponential Backoff */
    bool backoffauth;              /* Backoff by user, not IP address */
    char *backoffdb;               /* Directory for backoff databases */
    unsigned long backoffk;        /* Multiple for the sleep time */
    unsigned long backoffpostfast; /* Upper time limit for fast posting */
    unsigned long backoffpostslow; /* Lower time limit for slow posting */
    unsigned long backofftrigger;  /* Number of postings before triggered */

    /* Reading and posting -- TLS/SSL support */
    /* Do not test HAVE_OPENSSL.  This relieves customers of /usr/include/inn
     * from the need to guess whether INN was built with TLS/SSL support in
     * order to get a header that matches the installed libraries.
     */
    char *tlscafile;             /* Path to a certificate authority file */
    char *tlscapath;             /* Path to a directory of CA certificates */
    char *tlscertfile;           /* Path to the TLS/SSL certificate to use */
    char *tlskeyfile;            /* Path to the key for the certificate */
    char *tlsciphers;            /* OpenSSL-style cipher string */
    char *tlsciphers13;          /* OpenSSL-style cipher string for TLS 1.3 */
    bool tlscompression;         /* Turn TLS compression on/off */
    char *tlseccurve;            /* ECDH curve name */
    bool tlspreferserverciphers; /* Make server select the cipher */
    struct vector *tlsprotocols; /* List of supported TLS versions */

    /* Monitoring */
    bool doinnwatch;                  /* Start innwatch from rc.news? */
    unsigned long innwatchbatchspace; /* Minimum free space in pathoutgoing */
    unsigned long innwatchlibspace;   /* Minimum free space in pathdb */
    unsigned long innwatchloload;     /* Load times 100 at which to restart */
    unsigned long innwatchhiload;     /* Load times 100 at which to throttle */
    unsigned long innwatchpauseload;  /* Load times 100 at which to pause */
    unsigned long innwatchsleeptime;  /* Seconds to wait between checks */
    unsigned long innwatchspoolnodes; /* Minimum free inodes in patharticles */
    unsigned long innwatchspoolspace; /* Minimum free space in patharticles */

    /* Logging */
    bool docnfsstat; /* Run cnfsstat in the background? */
    bool htmlstatus; /* Write status file as HTML instead of plain text? */
    unsigned long
        incominglogfrequency; /* Checkpoint log after this many articles */
    bool logartsize;          /* Log article sizes? */
    bool logcancelcomm;       /* Log ctlinnd cancel commands to syslog? */
    unsigned long logcycles;  /* How many old logs scanlogs should keep */
    bool logipaddr;           /* Log by host IP address? */
    bool logsitename;         /* Log outgoing site names? */
    bool logstatus;           /* Send a status report to syslog? */
    bool logtrash;            /* Log unwanted newsgroups? */
    bool nnrpdoverstats;      /* Log overview statistics? */
    bool nntplinklog;         /* Put storage token into the log? */
    char *stathist;           /* Filename for history profiler outputs */
    unsigned long status;     /* Status file update interval */
    unsigned long timer;      /* Performance monitoring interval */

    /* System Tuning */
    unsigned long badiocount;    /* Failure count before dropping channel */
    unsigned long blockbackoff;  /* Multiplier for sleep in EAGAIN writes */
    unsigned long chaninacttime; /* Wait before noticing inactive channels */
    unsigned long chanretrytime; /* How long before channel restarts */
    unsigned long
        datamovethreshold;      /* Threshold to extend buffer or move data */
    unsigned long icdsynccount; /* Articles between active & history updates */
    unsigned long
        keepmmappedthreshold; /* Threshold for keeping mmap in buffindexed */
    unsigned long maxcmdreadsize; /* Max NNTP command read size used by innd */
    unsigned long maxforks;       /* Give up after this many fork failure. */
    unsigned long maxlisten;      /* Max listen() backlog */
    long nicekids;                /* Child processes get niced to this */
    unsigned long nicenewnews; /* If NEWNEWS command is used, nice to this */
    unsigned long nicennrpd;   /* nnrpd is niced to this */
    unsigned long pauseretrytime; /* Seconds before seeing if pause is ended */
    unsigned long peertimeout;    /* How long peers can be inactive */
    long rlimitnofile;            /* File descriptor limit to set */

    /* Paths */
    char *patharchive;  /* Archived news */
    char *patharticles; /* Articles */
    char *pathbin;      /* News binaries */
    char *pathcontrol;  /* Path to control message handlers */
    char *pathdb;       /* News database files */
    char *pathetc;      /* News configuration files */
    char *pathfilter;   /* Filtering code */
    char *pathhttp;     /* HTML files */
    char *pathincoming; /* Incoming spooled news */
    char *pathlog;      /* Log files */
    char *pathnews;     /* Home directory for news user */
    char *pathoutgoing; /* Outgoing news batch files */
    char *pathoverview; /* Overview infomation */
    char *pathrun;      /* Runtime state and sockets */
    char *pathspool;    /* Root of news spool hierarchy */
    char *pathtmp;      /* Temporary files for the news system */
};

/* The global innconf variable used in programs. */
extern struct innconf *innconf;

BEGIN_DECLS

/* Parse the given file into innconf, using the default path if NULL. */
bool innconf_read(const char *path);

/* Free an innconf struct and all allocated memory for it. */
void innconf_free(struct innconf *);

/* Print a single value with appropriate quoting, return whether found. */
bool innconf_print_value(FILE *, const char *key, enum confparse_quoting);

/* Dump the entire configuration with appropriate quoting. */
void innconf_dump(FILE *, enum confparse_quoting);

/* Compare two instances of an innconf struct, for testing. */
bool innconf_compare(struct innconf *, struct innconf *);

/* Check the validity of an inn.conf file.  Does innconf_read plus checking
   for any unknown parameters that are set. */
bool innconf_check(const char *path);

END_DECLS

#endif /* INN_INNCONF_H */
