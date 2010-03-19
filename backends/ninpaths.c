/*  $Id$
**
**  New inpaths reporting program.
**
**  Idea, data structures and part of code based on inpaths 2.5
**  by Brian Reid, Landon Curt Noll
**
**  This version written by Olaf Titz, Feb. 1997.  Public domain.
*/

#include "config.h"
#include "clibrary.h"
#include <ctype.h>
#include <time.h>

#define VERSION "3.1.1"

#define MAXFNAME 1024	/* max length of file name */
#define MAXLINE 1024	/* max length of Path line */
#define HASH_TBL 65536	/* hash table size (power of two) */
#define MAXHOST 128	/* max length of host name */
#define HOSTF "%127s"	/* scanf format for host name */
#define RECLINE 120	/* dump file line length softlimit */

/* structure used to tally the traffic between two hosts */
struct trec {
    struct trec *rlink;		/* next in chain */
    struct nrec *linkid;	/* pointer to... */
    long tally;			/* count */
};

/* structure to hold the information about a host */
struct nrec {
    struct nrec *link;		/* next in chain */
    struct trec *rlink;		/* start of trec chain */
    char *id;			/* host name */
    long no;			/* identificator for dump file */
    long sentto; 		/* tally of articles sent from here */
};

struct nrec *hosthash[HASH_TBL];

time_t starttime;	/* Start time */
double atimes=0.0;	/* Sum of articles times wrt. starttime */
long total=0, 		/* Total articles processed */
     sites=0;		/* Total sites known */

/* malloc and warn if out of mem */
static void *
wmalloc(size_t s)
{
    void *p=malloc(s);
    if (!p)
	fprintf(stderr, "warning: out of memory\n");
    return p;
}

/* Hash function due to Glenn Fowler / Landon Curt Noll / Phong Vo */
static int
hash(const char *str)
{
    unsigned long val;
    unsigned long c;

    for (val = 0; (c=(unsigned long)(*str)); ++str) {
	val *= 16777619;	/* magic */
	val ^= c;		/* more magic */
    }
    return (int)(val & (unsigned long)(HASH_TBL-1));
}

/* Look up a host in the hash table. Add if necessary. */
static struct nrec *
hhost(const char *n)
{
    struct nrec *h;
    int i=hash(n);

    for (h=hosthash[i]; h; h=h->link)
	if (!strcmp(n, h->id))
	    return h;
    /* not there - allocate */
    h=wmalloc(sizeof(struct nrec));
    if (!h)
	return NULL;
    h->id=strdup(n);
    if (!h->id) {
	free(h); return NULL;
    }
    h->link=hosthash[i];
    h->rlink=NULL;
    h->no=h->sentto=0;
    hosthash[i]=h;
    sites++;
    return h;
}

/* Look up a tally record between hosts. Add if necessary. */
static struct trec *
tallyrec(struct nrec *r, struct nrec *h)
{
    struct trec *t;
    for (t=r->rlink; t; t=t->rlink)
	if (t->linkid==h)
	    return t;
    t=wmalloc(sizeof(struct trec));
    if (!t)
	return NULL;
    t->rlink=r->rlink;
    t->linkid=h;
    t->tally=0;
    r->rlink=t;
    return t;
}


/* Dump file format:
   "!!NINP" <version> <starttime> <endtime> <sites> <total> <avgtime> "\n"
   followed by <sites> S-records,
   "!!NLREC\n"
   [3.0]
       followed by max. <sites>^2 L-records
   [3.1]
       followed by max. <sites> L-records
   "!!NEND" <nlrecs> "\n"
   starttime, endtime, avgtime as UNIX date
   the records are separated by space or \n
   an S-record is "site count"
   [3.0]
       an L-record is "sitea!siteb!count"
   [3.1]
       an L-record is ":sitea" { "!siteb,count" }...
       ",count" omitted if count==1
   where sitea and siteb are numbers of the S-records starting at 0
*/

static int
writedump(FILE *f)
{
    int i, j;
    long n;
    struct nrec *h;
    struct trec *t;

    if (!total) {
	return -1;
    }
    fprintf(f, "!!NINP " VERSION " %lu %lu %ld %ld %ld\n",
            (unsigned long) starttime, (unsigned long) time(NULL), sites,
            total, (long)(atimes/total)+starttime);
    n=j=0;
    /* write the S-records (hosts), numbering them in the process */
    for (i=0; i<HASH_TBL; ++i)
	for (h=hosthash[i]; h; h=h->link) {
	    h->no=n++;
	    j+=fprintf(f, "%s %ld", h->id, h->sentto);
	    if (j>RECLINE) {
		j=0;
		fprintf(f, "\n");
	    } else {
		fprintf(f, " ");
	    }
	}
    if (n!=sites)
	fprintf(stderr, "internal error: sites=%ld, dumped=%ld\n", sites, n);

    fprintf(f, "\n!!NLREC\n");

    n=j=0;
    /* write the L-records (links) */
    for (i=0; i<HASH_TBL; ++i)
	for (h=hosthash[i]; h; h=h->link)
	    if ((t=h->rlink)) {
		j+=fprintf(f, ":%ld", h->no);
		for (; t; t=t->rlink) {
		    j+=fprintf(f, "!%ld", t->linkid->no);
		    if (t->tally>1)
			j+=fprintf(f, ",%ld", t->tally);
		    n++;
		}
		if (j>RECLINE) {
		    j=0;
		    fprintf(f, "\n");
		}
	    }
    fprintf(f, "\n!!NLEND %ld\n", n);
    return 0;
}

/* Write dump to a named file. Substitute %d in file name with system time. */

static void
writedumpfile(const char *n)
{
    char buf[MAXFNAME];
    FILE *d;

    if (n[0]=='-' && n[1]=='\0') {
	writedump(stdout);
	return;
    }
    snprintf(buf, sizeof(buf), n, time(0));
    d=fopen(buf, "w");
    if (d) {
	if (writedump(d)<0)
            unlink(buf);
    } else {
	perror("writedumpfile: fopen");
    }
}

/* Read a dump file. */

static int
readdump(FILE *f)
{
    int a, b;
    long i, m, l;
    unsigned long st, et, at;
    long sit, tot;
    struct nrec **n;
    struct trec *t;
    char c[MAXHOST];
    char v[16];

    #define formerr(i) {\
	fprintf(stderr, "dump file format error #%d\n", (i)); return -1; }

    if (fscanf(f, "!!NINP %15s %lu %lu %ld %ld %lu\n",
	       v, &st, &et, &sit, &tot, &at)!=6)
	formerr(0);

    n=calloc(sit, sizeof(struct nrec *));
    if (!n) {
	fprintf(stderr, "error: out of memory\n");
	return -1;
    }
    for (i=0; i<sit; i++) {
	if (fscanf(f, HOSTF " %ld ", c, &l)!=2) {
	    fprintf(stderr, "read %ld ", i);
	    formerr(1);
	}
	n[i]=hhost(c);
        if (!n[i])
            return -1;
        n[i]->sentto+=l;
    }
    if ((fscanf(f, HOSTF "\n", c)!=1) ||
	strcmp(c, "!!NLREC"))
	formerr(2);
    m=0;
    if (!strncmp(v, "3.0", 3)) {
	/* Read 3.0-format L-records */
	while (fscanf(f, "%d!%d!%ld ", &a, &b, &l)==3) {
	    t=tallyrec(n[a], n[b]);
	    if (!t)
                return -1;
            t->tally+=l;
	    ++m;
	}
    } else if (!strncmp(v, "3.1", 3)) {
	/* Read L-records */
	while (fscanf(f, " :%d", &a)==1) {
	    while ((i=fscanf(f, "!%d,%ld", &b, &l))>0) {
		t=tallyrec(n[a], n[b]);
		if (i<2)
		    l=1;
		if (!t)
                    return -1;
                t->tally+=l;
		++m;
	    }
	}
    } else {
	fprintf(stderr, "version %s ", v);
	formerr(9);
    }
    if ((fscanf(f, "!!NLEND %ld\n", &i)!=1)
	|| (i!=m))
	formerr(3);
#ifdef DEBUG
    {
        time_t st_time = st;
        fprintf(stderr, " dumped start %s   total=%ld atimes=%ld (%ld)\n",
                ctime(&st_time), tot, at, at-st);
    }
#endif
    /* Adjust the time average and total count */
    if ((unsigned long) starttime > st) {
	atimes+=(double)total*(starttime-st);
	starttime=st;
    }
    atimes+=(double)tot*(at-starttime);
    total+=tot;
#ifdef DEBUG
    fprintf(stderr, " current start %s   total=%ld atimes=%.0f (%.0f)\n\n",
            ctime(&starttime), total, atimes, atimes/total);
#endif
    free(n);
    return 0;
}

/* Read dump from a file. */

static int
readdumpfile(const char *n)
{
    FILE *d;
    int i;

    if (n[0]=='-' && n[1]=='\0')
	return readdump(stdin);

    d=fopen(n, "r");
    if (d) {
	/* fprintf(stderr, "Reading dump file %s\n", n); */
	i=readdump(d);
	fclose(d);
	return i;
    } else {
	perror("readdumpfile: fopen");
	return -1;
    }
}


/* Process a Path line. */

static int
pathline(char *c)
{
    char *c2;
    struct nrec *h, *r;
    struct trec *t;

    r=NULL;
    while (*c) {
	for (c2=c; *c2 && *c2!='!'; c2++);
	if (c2-c>MAXHOST-1)
	    /* looks broken, dont bother with rest */
	    return 0;
	while (*c2=='!')
	    *c2++='\0'; /* skip "!!" too */
	h=hhost(c);
        if (!h)
            return -1;
        ++h->sentto;
        if (r && r!=h) {
            t=tallyrec(r, h);
            if (!t)
                return -1;
            ++t->tally;
	}
	c=c2;
	r=h;
    }
    return 0;
}

/* Take Path lines from file (stdin used here). */

static void
procpaths(FILE *f)
{
    char buf[MAXLINE];
    char *c, *ce;
    int v=1; /* current line is valid */

    while (fgets(buf, sizeof(buf), f)) {
	c=buf;
	if (!strncasecmp(c, "Path: ", 6))
	    c+=6;
	/* find end of line. Some broken newsreaders preload Path with
	   a name containing spaces. Chop off those entries. */
	for (ce=c; *ce && !isspace((unsigned char) *ce); ++ce);
	if (!*ce) {
	    /* bogus line */
	    v=0;
	} else if (v) {
	    /* valid line */
	    for (; ce>c && *ce!='!'; --ce); /* ignore last element */
	    *ce='\0';
	    if (pathline(c)<0) /* process it */
                /* If an out of memory condition occurs while reading
                   Path lines, stop reading and write the dump so far.
                   INN will restart a fresh ninpaths. */
                return;
	    /* update average age and grand total */
	    atimes+=(time(0)-starttime);
	    ++total;
	} else {
	    /* next line is valid */
	    v=1;
	}
    }
}

/* Output a report suitable for mailing. From inpaths 2.5 */

static void
report(const char *hostname, int verbose)
{
    double avgAge;
    int i, columns, needHost;
    long nhosts=0, nlinks=0;
    struct nrec *list, *relay;
    struct trec *rlist;
    char hostString[MAXHOST];
    time_t t0=time(0);

    if (!total) {
	fprintf(stderr, "report: no traffic\n");
	return;
    }
    /* mark own site to not report it */
    list=hhost(hostname);
    if (list)
	list->id[0]='\0';

    avgAge=((double)t0 - (atimes/total + (double)starttime)) /86400.0;
    printf("ZCZC begin inhosts %s %s %d %ld %3.1f\n",
    	VERSION,hostname,verbose,total,avgAge);
    for (i=0; i<HASH_TBL-1; i++) {
	list = hosthash[i];
	while (list != NULL) {
	    if (list->id[0] != 0 && list->rlink != NULL) {
		if (verbose > 0 || (100*list->sentto > total))
		    printf("%ld\t%s\n",list->sentto, list->id);
	    }
	    list = list->link;
	}
    }
    printf("ZCZC end inhosts %s\n",hostname);

    printf("ZCZC begin inpaths %s %s %d %ld %3.1f\n",
        VERSION,hostname,verbose,total,avgAge);
    for (i=0; i<HASH_TBL-1; i++) {
	list = hosthash[i];
	while (list != NULL) {
	    if (verbose > 1 || (100*list->sentto > total)) {
		if (list->id[0] != 0 && list->rlink != NULL) {
		    columns = 3+strlen(list->id);
		    snprintf(hostString,sizeof(hostString),"%s H ",list->id);
		    needHost = 1;
		    rlist = list->rlink;
		    while (rlist != NULL) {
		        if (
			     (100*rlist->tally > total)
			  || ((verbose > 1)&&(5000*rlist->tally>total))
			   ) {
			    if (needHost) printf("%s",hostString);
			    needHost = 0;
			    relay = rlist->linkid;
			    if (relay->id[0] != 0) {
			      if (columns > 70) {
				printf("\n%s",hostString);
				columns = 3+strlen(list->id);
			      }
			      printf("%ld Z %s U ", rlist->tally, relay->id);
			      columns += 9+strlen(relay->id);
			    }
			}
			rlist = rlist->rlink;
                        ++nlinks;
		    }
		    if (!needHost) printf("\n");
		}
	    }
	    list = list->link;
            ++nhosts;
	}
    }
    printf("ZCZC end inpaths %s\n",hostname);
#ifdef DEBUG
    fprintf(stderr, "Processed %ld hosts, %ld links.\n", nhosts, nlinks);
#endif
}

int
main(int argc, char *argv[])
{
    int i;
    int pf=0, vf=2;
    char *df=NULL, *rf=NULL;

    for (i=0; i<HASH_TBL; i++)
	hosthash[i]=NULL;
    starttime=time(0);

    while ((i=getopt(argc, argv, "pd:u:r:v:"))!=EOF)
	switch (i) {
	case 'p':
	    /* read Path lines from stdin */
	    pf=1; break;
	case 'd':
	    /* make a dump to the named file */
	    df=optarg; break;
	case 'u':
	    /* read dump from the named file */
	    if (readdumpfile(optarg)<0)
		exit(1);
	    break;
	case 'r':
	    /* make a report for the named site */
	    rf=optarg; break;
	case 'v':
	    /* control report verbosity */
	    vf=atoi(optarg); break;
	default:
	    fprintf(stderr, "unknown option %c\n", i);
	}

    if (pf)
	procpaths(stdin);
    if (df)
	writedumpfile(df);
    if (rf)
	report(rf, vf);
    return 0;
}
