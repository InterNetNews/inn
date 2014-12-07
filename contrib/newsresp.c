/* newsresp.c - EUnet - bilse */

/*
 * From: Koen De Vleeschauwer <koen@eu.net>
 * Subject: Re: innfeed-users: innfeed: measuring server response time
 * To: jeff.garzik@spinne.com (Jeff Garzik)
 * Date: Tue, 13 May 1997 16:33:27 +0200 (MET DST)
 * Cc: innfeed-users@vix.com
 * 
 * > Is there an easy way to measure server response time, and print it out
 * > on the innfeed status page?  Cyclone's nntpTime measures login banner
 * > response time and an article add and lookup operation. 
 * > 
 * > It seems to me that innfeed could do something very similar.  It could
 * > very easily sample gettimeofday() or Time.Now to determine a remote
 * > server's average response time for lookups, lookup failures, article
 * > send throughput, whatever.
 * > 
 * > These statistics might be invaluable to developers creating advanced
 * > connection and article delivery algorithms.  If I knew, for example,
 * > that a site's article send/save throughput was really fast, but history
 * > lookups were really slow, my algorithm could reserve a channel or two
 * > for TAKETHIS-only use.
 * 
 * We use a stand-alone program which opens up an additional nntp channel
 * from time to time and takes a peek at the various response times.
 * It's also interesting to tune one's own box.
 * I've included the source code; please consider this supplied 'as is';
 * bugs and features alike. SunOS, Solaris and Irix ought to be ok;
 * eg. gcc -traditional -o newsresp ./newsresp.c -lnsl -lsocket on S0laris.
 * If a host has an uncommonly long banner you may have to change a constant 
 * somewhere; forget. Please note one has to interpret the output; 
 * eg. whether one is measuring rtt or history lookup time.
 * 
 * Basic usage is:
 * news 1 % newsresp -n 5 news.eu.net
 * ---------------------------------
 * news.eu.net is 134.222.90.2 port 119
 *  elap  diff
 *   0.0   0.0  Connecting ...
 *   0.0   0.0  OK, waiting for prompt
 *   0.0   0.0  <<< 200 EU.net InterNetNews server INN 1.5.1 17-Dec-1996 re [...]
 *   0.0   0.0  >>> ihave <244796399@a>
 *   0.0   0.0  <<< 335 
 *   0.0   0.0  >>> .
 *   0.0   0.0  <<< 437 Empty article 
 *   0.0   0.0  >>> ihave <244796398@a>
 *   0.0   0.0  <<< 335 
 *   0.0   0.0  >>> .
 *   0.0   0.0  <<< 437 Empty article 
 *   0.0   0.0  >>> ihave <244796397@a>
 *   0.0   0.0  <<< 335 
 *   0.0   0.0  >>> .
 *   0.0   0.0  <<< 437 Empty article 
 *   0.0   0.0  >>> ihave <244796396@a>
 *   0.1   0.0  <<< 335 
 *   0.1   0.0  >>> .
 *   0.1   0.0  <<< 437 Empty article 
 *   0.1   0.0  >>> ihave <244796395@a>
 *   0.1   0.0  <<< 335 
 *   0.1   0.0  >>> .
 *   0.1   0.0  <<< 437 Empty article 
 *   0.1   0.0  >>> quit
 *   0.1   0.0  <<< 205 . 
 */

#include "clibrary.h"
#include "portable/socket.h"

#include <errno.h>
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#include <time.h>

#define NNTPPORT 119

void error(const char *);
void fatal(const char *);
void ierror(const char *, const char *);
void ifatal(const char *, const char *);
unsigned int do_time(unsigned int);
void ptime(void);
void massagebuff(int, char *);
bool punt(int);

struct sockaddr_in sock_in;
int sock;
char buf[1024];

int
main(int argc, char *argv[])
{
  int errflg = 0, c;
  bool status = true;
  struct hostent *host;
  unsigned long temp;
  unsigned numart = 1;
  struct protoent *tcp_proto;
  char **whoP;

  while ( (c = getopt(argc,argv,"n:")) != -1 )
    switch ( c ) {
    case 'n': sscanf(optarg,"%u",&numart); break;
    default : errflg++;
  }
  if ( numart == 0 || optind == argc )
    errflg++;
  if ( errflg ) {
    fprintf(stderr,"Usage: %s [-n articles] host ...\n",argv[0]);
    exit(1);
  }

  if ( (tcp_proto = getprotobyname("tcp")) == 0 )
    fatal("getprotobyname");
  for ( whoP = argv+optind; *whoP != 0; whoP++ ) {
    if ( (sock = socket(PF_INET,SOCK_STREAM,tcp_proto->p_proto)) < 0 )
      fatal("socket");
    temp = inet_addr(*whoP);
    if ( temp != (unsigned long) -1 ) {
      sock_in.sin_addr.s_addr = temp;
      sock_in.sin_family = AF_INET;
    }
    else {
      host = gethostbyname(*whoP);
      if ( host ) {
	sock_in.sin_family = host->h_addrtype;
	memcpy(&sock_in.sin_addr,host->h_addr,host->h_length);
      }
      else {
	fprintf(stderr,"gethostbyname can't find %s\n",*whoP);
	exit(1);
      }
    }
    sock_in.sin_port = htons(NNTPPORT);
    printf("---------------------------------\n%s is %s port %d\n",
	       *whoP,inet_ntoa(sock_in.sin_addr),ntohs(sock_in.sin_port));
    status = punt(numart);
    close(sock);
  }
  if (status) {
      exit(0);
  } else {
      exit(1);
  }
}

void
error(const char *what)
{
  ptime();
  fflush(stdout);
  perror(what);
}

void
fatal(const char *what)
{
  error(what);
  exit(2);
}

void
ierror(const char *how, const char *what)
{
  printf("Expected %s, received %s; bailing out.\n", how, what);
}

void
ifatal(const char *how, const char *what)
{
  ierror(how, what);
  exit(1);
}

unsigned int
do_time(unsigned int start)
{
  struct timeval now;

  gettimeofday(&now,(struct timezone *)0);
  return ( now.tv_sec*1000 + now.tv_usec/1000 - start );
}

unsigned int start, elapsed, diff;

void
ptime(void)
{
  diff = elapsed;
  elapsed = do_time(start);
  diff = elapsed - diff;
  printf("%5.1f %5.1f  ",((float)elapsed)/1000.0,((float)diff)/1000.0);
}

void
massagebuff(int bread, char *buf)
{
  char *p;

  if ( bread > 55 )
    strcpy(buf+55," [...]\n");
  else
    buf[bread] = '\0';
  for ( p = buf; *p != '\0'; )
    if ( *p != '\r' )  /* We like to do it RISC style. */
      p++;
    else {
      *p = ' ';
      p++;
    }
}

bool
punt(int numart)
{
  static char ihave[32],
	      dot[] = ".\r\n",
	      quit[] = "quit\r\n";
  struct timeval start_tv;
  int bread;

  printf(" elap  diff\n");
  diff = elapsed = 0;
  gettimeofday(&start_tv,(struct timezone *)0);
  start = start_tv.tv_sec*1000 + start_tv.tv_usec/1000;

  ptime();
  printf("Connecting ...\n");
  if ( connect(sock,(struct sockaddr*)&sock_in,sizeof(sock_in)) < 0 ) {
    error("connect");
    return false;
  }
  ptime();
  printf("OK, waiting for prompt\n");

  if ( (bread=read(sock,buf,sizeof(buf))) < 0 ) {
    error("read socket");
    return false;
  }
  massagebuff(bread,buf);
  ptime();
  printf("<<< %s",buf);
  if ( strncmp(buf,"200",3) != 0 && strncmp(buf,"201",3) != 0 ) {
    ierror("200 or 201",buf);
    return false;
  }

  do {
    snprintf(ihave,sizeof(ihave),"ihave <%u@a>\r\n",start+numart);
    ptime();
    printf(">>> %s",ihave);
    if ( write(sock,ihave,strlen(ihave)) != (int) strlen(ihave) ) {
      error("write socket");
      return false;
    }

    if ( (bread=read(sock,buf,sizeof(buf))) < 0 ) {
      error("read socket");
      return false;
    }
    massagebuff(bread,buf);
    ptime();
    printf("<<< %s",buf);
    if ( strncmp(buf,"335",3) != 0 && strncmp(buf,"435",3) != 0 ) {
      ierror("335 or 435",buf);
      return false;
    }

    if ( strncmp(buf,"335",3) == 0 ) {
      ptime();
      printf(">>> %s",dot);
      if ( write(sock,dot,sizeof(dot)-1) != sizeof(dot)-1 ) {
	error("write socket");
	return false;
      }

      if ( (bread=read(sock,buf,sizeof(buf))) < 0 ) {
	error("read socket");
	return false;
      }
      massagebuff(bread,buf);
      ptime();
      printf("<<< %s",buf);
      if ( strncmp(buf,"437",3) != 0 && strncmp(buf,"235",3) != 0 ) {
	ierror("437 or 235",buf);
	return false;
      }
    }
  } while ( --numart != 0 );

  ptime();
  printf(">>> %s",quit);
  if ( write(sock,quit,sizeof(quit)-1) != sizeof(quit)-1 ) {
    error("write socket");
    return false;
  }

  if ( (bread=read(sock,buf,sizeof(buf))) < 0 ) {
    error("read socket");
    return false;
  }
  massagebuff(bread,buf);
  ptime();
  printf("<<< %s",buf);
  if ( strncmp(buf,"205",3) != 0 ) {
    ierror("205",buf);
    return false;
  }
  return true;
}
