/* newsresp.c - EUnet - bilse */
#define FD_SETSIZE 32

#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>

#define NNTPPORT 119
struct sockaddr_in sock_in;
int sock;
char buf[1024];

main(argc,argv)
int argc;
char *argv[];
{
  int errflg = 0, c;
  extern char *optarg;
  extern int optind;
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
  close(fileno(stderr));
  fileno(stderr) = dup(fileno(stdout));

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
#if defined(h_addr)         /* In 4.3, this is a #define */
	memcpy((caddr_t)&sock_in.sin_addr,
			    host->h_addr_list[0],host->h_length);
#else
	memcpy((caddr_t)&sock_in.sin_addr,host->h_addr,host->h_length);
#endif
      }
      else {
	fprintf(stderr,"gethostbyname can't find %s\n",*whoP);
	exit(1);
      }
    }
    sock_in.sin_port = htons(NNTPPORT);
    printf("---------------------------------\n%s is %s port %d\n",
	       *whoP,inet_ntoa(sock_in.sin_addr),ntohs(sock_in.sin_port));
    punt(numart);
    close(sock);
  }
}

error(what)
char *what;
{
  ptime(); fflush(stdout);
  perror(what);
}

fatal(what)
char *what;
{
  error(what);
  exit(2);
}

ierror(how,what)
char *how, *what;
{
  printf("Expected %s, bailing out.\n",how);
}

ifatal(how,what)
char *how, *what;
{
  ierror(how,what);
  exit(1);
}

unsigned do_time(start)
unsigned start;
{
  struct timeval now;

  gettimeofday(&now,(struct timezone *)0);
  return ( now.tv_sec*1000 + now.tv_usec/1000 - start );
}


unsigned start, elapsed, diff;

ptime()
{
  diff = elapsed;
  elapsed = do_time(start);
  diff = elapsed - diff;
  printf("%5.1f %5.1f  ",((float)elapsed)/1000.0,((float)diff)/1000.0);
}

massagebuff(bread,buf)
int bread;
char *buf;
{
  register char *p;

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

punt(numart)
int numart;
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
    return(-1);
  }
  ptime();
  printf("OK, waiting for prompt\n");

  if ( (bread=read(sock,buf,sizeof(buf))) < 0 ) {
    error("read socket");
    return(-1);
  }
  massagebuff(bread,buf);
  ptime();
  printf("<<< %s",buf);
  if ( strncmp(buf,"200",3) != 0 && strncmp(buf,"201",3) != 0 ) {
    ierror("200 or 201",buf);
    return(-1);
  }

  do {
    sprintf(ihave,"ihave <%u@a>\r\n",start+numart);
    ptime();
    printf(">>> %s",ihave);
    if ( write(sock,ihave,strlen(ihave)) != strlen(ihave) ) {
      error("write socket");
      return(-1);
    }

    if ( (bread=read(sock,buf,sizeof(buf))) < 0 ) {
      error("read socket");
      return(-1);
    }
    massagebuff(bread,buf);
    ptime();
    printf("<<< %s",buf);
    if ( strncmp(buf,"335",3) != 0 && strncmp(buf,"435",3) != 0 ) {
      ierror("335 or 435 ",buf);
      return(-1);
    }

    if ( strncmp(buf,"335",3) == 0 ) {
      ptime();
      printf(">>> %s",dot);
      if ( write(sock,dot,sizeof(dot)-1) != sizeof(dot)-1 ) {
	error("write socket");
	return(-1);
      }

      if ( (bread=read(sock,buf,sizeof(buf))) < 0 ) {
	error("read socket");
	return(-1);
      }
      massagebuff(bread,buf);
      ptime();
      printf("<<< %s",buf);
      if ( strncmp(buf,"437",3) != 0 && strncmp(buf,"235",3) != 0 ) {
	ierror("437 or 235",buf);
	return(-1);
      }
    }
  } while ( --numart != 0 );

  ptime();
  printf(">>> %s",quit);
  if ( write(sock,quit,sizeof(quit)-1) != sizeof(quit)-1 ) {
    error("write socket");
    return(-1);
  }

  if ( (bread=read(sock,buf,sizeof(buf))) < 0 ) {
    error("read socket");
    return(-1);
  }
  massagebuff(bread,buf);
  ptime();
  printf("<<< %s",buf);
  if ( strncmp(buf,"205",3) != 0 ) {
    ierror("205",buf);
    return(-1);
  }
  return(0);
}
