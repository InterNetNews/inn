/*  $Id$
**
**  Authenticate a user against a remote radius server.
*/

#include "config.h"
#include "clibrary.h"
#include "portable/time.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>

/* Needed on AIX 4.1 to get fd_set and friends. */
#if HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif

#include "inn/innconf.h"
#include "inn/md5.h"
#include "inn/messages.h"
#include "inn/libinn.h"
#include "nntp.h"
#include "inn/paths.h"
#include "conffile.h"

#include "libauth.h"

#define RADIUS_LOCAL_PORT       NNTP_PORT

#define AUTH_VECTOR_LEN 16

typedef struct _auth_req {
    unsigned char	code;
    unsigned char	id;
    unsigned short	length;
    unsigned char	vector[AUTH_VECTOR_LEN];
    unsigned char	data[NNTP_STRLEN*2];
    int			datalen;
} auth_req;

typedef struct _rad_config_t {
    char *secret;	/* pseudo encryption thingy secret that radius uses */

    char *radhost;	/* parameters for talking to the remote radius sever */
    int radport;
    char *lochost;
    int locport;

    char *prefix, *suffix;	/* futz with the username, if necessary */
    int ignore_source;

    struct _rad_config_t *next;  /* point to any additional servers */
} rad_config_t;
  
typedef struct _sending_t {
    auth_req req;
    int reqlen;
    struct sockaddr_in sinr;
    struct _sending_t *next;
} sending_t;

#define RADlbrace  1
#define RADrbrace  2
#define RADserver  10
#define RADhost    11
#define RADsecret  12
#define RADport    13
#define RADlochost 14
#define RADlocport 15
#define RADprefix  16
#define RADsuffix  17
#define RADsource  18

static CONFTOKEN radtoks[] = {
    { RADlbrace,        (char *) "{"                    },
    { RADrbrace,        (char *) "}"                    },
    { RADserver,        (char *) "server"               },
    { RADhost,          (char *) "radhost:"             },
    { RADsecret,        (char *) "secret:"              },
    { RADport,          (char *) "radport:"             },
    { RADlochost,       (char *) "lochost:"             },
    { RADlocport,       (char *) "locport:"             },
    { RADprefix,        (char *) "prefix:"              },
    { RADsuffix,        (char *) "suffix:"              },
    { RADsource,        (char *) "ignore-source:"       },
    { 0,                NULL                            }
};

static rad_config_t *get_radconf(void)
{
  rad_config_t *new;

  new = xcalloc(1, sizeof(rad_config_t));
  new->next = NULL;

  return new;
}

static int read_config(char *authfile, rad_config_t *radconf)
{
    int inbrace;
    rad_config_t *radconfig=NULL;
    CONFFILE *file;
    CONFTOKEN *token;
    char *server;
    int type;
    char *iter;

    if ((file = CONFfopen(authfile)) == NULL)
      sysdie("cannot open config file %s", authfile);

    inbrace = 0;
    while ((token = CONFgettoken(radtoks, file)) != NULL) {
      if (!inbrace) {
	if (token->type != RADserver)
          die("expected server keyword on line %d", file->lineno);
	if ((token = CONFgettoken(0, file)) == NULL)
          die("expected server name on line %d", file->lineno);
	server = xstrdup(token->name);
	if ((token = CONFgettoken(radtoks, file)) == NULL 
	    || token->type != RADlbrace)
          die("expected { on line %d", file->lineno);
	inbrace = 1;

	if (radconfig == NULL)
	  radconfig = radconf;
	else {
	  radconfig->next = get_radconf();
	  radconfig = radconfig->next;
	}
      }
      else {
	type = token->type;
	if (type == RADrbrace)
	  inbrace = 0;
	else {
	  if ((token = CONFgettoken(0, file)) == NULL)
            die("keyword with no value on line %d", file->lineno);
	  iter = token->name;

	  /* what are we setting? */
	  switch(type) {
	  case RADsecret:
	    if (radconfig->secret) continue;
	    radconfig->secret = xstrdup(iter);
	    break;
	  case RADhost:
	    if (radconfig->radhost) continue;
	    radconfig->radhost = xstrdup(iter);
	    break;
	  case RADport:
	    if (radconfig->radport) continue;
	    radconfig->radport = atoi(iter);
	    break;
	  case RADlochost:
	    if (radconfig->lochost) continue;
	    radconfig->lochost = xstrdup(iter);
	    break;
	  case RADlocport:
	    if (radconfig->locport) continue;
	    radconfig->locport = atoi(iter);
	    break;
	  case RADprefix:
	    if (radconfig->prefix) continue;
	    radconfig->prefix = xstrdup(iter);
	    break;
	  case RADsuffix:
	    if (radconfig->suffix) continue;
	    radconfig->suffix = xstrdup(iter);
	    break;
	  case RADsource:
	    if (!strcasecmp(iter, "true"))
		radconfig->ignore_source = 1;
	    else if (!strcasecmp(iter, "false"))
		radconfig->ignore_source = 0;
	    else
                die("expected true or false after ignore-source on line %d",
                    file->lineno);
	    break;
	  default:
            die("unknown keyword on line %d", file->lineno);
	  }
	}
      }
    }

    CONFfclose(file);

    if (!radconf->radhost)
        die("no radius host specified");
    else if (!radconf->secret)
        die("no shared secret with radius host specified");

    return(0);
}

#define PW_AUTH_UDP_PORT 1645

#define PW_AUTHENTICATION_REQUEST 1
#define PW_AUTHENTICATION_ACK     2
#define PW_AUTHENTICATION_REJECT  3

#define PW_USER_NAME            1
#define PW_PASSWORD             2

#define PW_SERVICE_TYPE         6
#define PW_SERVICE_AUTH_ONLY    8

#define RAD_NAS_IP_ADDRESS      4       /* IP address */
#define RAD_NAS_PORT            5       /* Integer */

static void req_copyto (auth_req to, sending_t *from)
{
    to = from->req;
}

static void req_copyfrom (sending_t *to, auth_req from)
{
    to->req = from;
}

static int rad_auth(rad_config_t *radconfig, char *uname, char *pass)
{
    auth_req req;
    int i, j, jlen, passstart;
    unsigned char secbuf[128];
    char hostname[SMBUF];
    unsigned char digest[MD5_DIGESTSIZE];
    struct timeval seed;
    struct sockaddr_in sinl;
    int sock;
    struct hostent *hent;
    int passlen;
    time_t now, end;
    struct timeval tmout;
    int got;
    fd_set rdfds;
    uint32_t nvalue;
    socklen_t slen;
    int authtries= 3; /* number of times to try reaching the radius server */
    rad_config_t *config;
    sending_t *reqtop, *sreq, *new;
    int done;

    /* set up the linked list */
    config = radconfig;

    if (config == NULL) {
      warn("no configuration file");
      return(-2);
    } else {
      /* setting sreq to NULL guarantees reqtop will be properly set later */
      sreq = NULL;
      reqtop = NULL;
    }

    while (config != NULL){
      new = xmalloc(sizeof(sending_t));
      new->next = NULL;

      if (sreq == NULL){
	reqtop = new;
	sreq = new;
      } else {
	sreq->next = new;
	sreq = sreq->next;
      }
      req_copyto(req, sreq);
  
      /* first, build the sockaddrs */
      memset(&sinl, '\0', sizeof(sinl));
      memset(&sreq->sinr, '\0', sizeof(sreq->sinr));
      sinl.sin_family = AF_INET;
      sreq->sinr.sin_family = AF_INET;
      if (config->lochost == NULL) {
        if (gethostname(hostname, sizeof(hostname)) != 0) {
          syswarn("cannot get local hostname");
          return(-2);
        }
	config->lochost = xstrdup(hostname);
      }
      if (config->lochost) {
	if (inet_aton(config->lochost, &sinl.sin_addr) != 1) {
          if ((hent = gethostbyname(config->lochost)) == NULL) {
            warn("cannot gethostbyname lochost %s", config->lochost);
            return(-2);
          }
          memcpy(&sinl.sin_addr.s_addr, hent->h_addr,
                 sizeof(struct in_addr));
	}
      }
      if (inet_aton(config->radhost, &sreq->sinr.sin_addr) != 1) {
        if ((hent = gethostbyname(config->radhost)) == NULL) {
          warn("cannot gethostbyname radhost %s", config->radhost);
          return(-2);
        }
	memcpy(&sreq->sinr.sin_addr.s_addr, hent->h_addr_list[0],
               sizeof(struct in_addr));
      }

      if (config->radport)
        sreq->sinr.sin_port = htons(config->radport);
      else
	sreq->sinr.sin_port = htons(PW_AUTH_UDP_PORT);

      /* seed the random number generator for the auth vector */
      gettimeofday(&seed, 0);
      srandom((unsigned) seed.tv_sec+seed.tv_usec);
      /* build the visible part of the auth vector randomly */
      for (i = 0; i < AUTH_VECTOR_LEN; i++)
	req.vector[i] = random() % 256;
      strlcpy((char *) secbuf, config->secret, sizeof(secbuf));
      memcpy(secbuf+strlen(config->secret), req.vector, AUTH_VECTOR_LEN);
      md5_hash(secbuf, strlen(config->secret)+AUTH_VECTOR_LEN, digest);
      /* fill in the auth_req data */
      req.code = PW_AUTHENTICATION_REQUEST;
      req.id = 0;

      /* bracket the username in the configured prefix/suffix */
      req.data[0] = PW_USER_NAME;
      req.data[1] = 2;
      req.data[2] = '\0';
      if (config->prefix) {
	req.data[1] += strlen(config->prefix);
	strlcat((char *) &req.data[2], config->prefix, sizeof(req.data) - 2);
      }
      req.data[1] += strlen(uname);
      strlcat((char *)&req.data[2], uname, sizeof(req.data) - 2);
      if (!strchr(uname, '@') && config->suffix) {
	req.data[1] += strlen(config->suffix);
	strlcat((char *)&req.data[2], config->suffix, sizeof(req.data) - 2);
      }
      req.datalen = req.data[1];

      /* set the password */
      passstart = req.datalen;
      req.data[req.datalen] = PW_PASSWORD;
      /* Null pad the password */
      passlen = (strlen(pass) + 15) / 16;
      passlen *= 16;
      req.data[req.datalen+1] = passlen+2;
      strlcpy((char *)&req.data[req.datalen+2], pass,
              sizeof(req.data) - req.datalen - 2);
      passlen -= strlen(pass);
      while (passlen--)
	req.data[req.datalen+passlen+2+strlen(pass)] = '\0';
      req.datalen += req.data[req.datalen+1];

      /* Add NAS_PORT and NAS_IP_ADDRESS into request */
      if ((nvalue = config->locport) == 0)
        nvalue = RADIUS_LOCAL_PORT;
      req.data[req.datalen++] = RAD_NAS_PORT;
      req.data[req.datalen++] = sizeof(nvalue) + 2;
      nvalue = htonl(nvalue);
      memcpy(req.data + req.datalen, &nvalue, sizeof(nvalue));
      req.datalen += sizeof(nvalue);
      req.data[req.datalen++] = RAD_NAS_IP_ADDRESS;
      req.data[req.datalen++] = sizeof(struct in_addr) + 2;
      memcpy(req.data + req.datalen, &sinl.sin_addr.s_addr,
           sizeof(struct in_addr));
      req.datalen += sizeof(struct in_addr);

      /* we're only doing authentication */
      req.data[req.datalen] = PW_SERVICE_TYPE;
      req.data[req.datalen+1] = 6;
      req.data[req.datalen+2] = (PW_SERVICE_AUTH_ONLY >> 24) & 0x000000ff;
      req.data[req.datalen+3] = (PW_SERVICE_AUTH_ONLY >> 16) & 0x000000ff;
      req.data[req.datalen+4] = (PW_SERVICE_AUTH_ONLY >> 8) & 0x000000ff;
      req.data[req.datalen+5] = PW_SERVICE_AUTH_ONLY & 0x000000ff;
      req.datalen += req.data[req.datalen+1];

      /* filled in the data, now we know what the actual length is. */
      req.length = 4+AUTH_VECTOR_LEN+req.datalen;

      /* "encrypt" the password */
      for (i = 0; i < req.data[passstart+1]-2; i += sizeof(HASH)) {
	jlen = sizeof(HASH);
	if (req.data[passstart+1]-(unsigned)i-2 < sizeof(HASH))
	    jlen = req.data[passstart+1]-i-2;
	for (j = 0; j < jlen; j++)
	    req.data[passstart+2+i+j] ^= digest[j];
	if (jlen == sizeof(HASH)) {
	    /* Recalculate the digest from the HASHed previous */
	    strlcpy((char *) secbuf, config->secret, sizeof(secbuf));
	    memcpy(secbuf+strlen(config->secret), &req.data[passstart+2+i],
                   sizeof(HASH));
            md5_hash(secbuf, strlen(config->secret)+sizeof(HASH), digest);
	}
      }
      sreq->reqlen = req.length;
      req.length = htons(req.length);

      req_copyfrom(sreq, req);

      /* Go to the next record in the list */
      config = config->next;
    }

    /* YAYY! The auth_req is ready to go! Build the reply socket and send out
     * the message. */

    /* now, build the sockets */
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
      syswarn("cannot build reply socket");
      return(-1);
    }
    if (bind(sock, (struct sockaddr*) &sinl, sizeof(sinl)) < 0) {
      syswarn("cannot bind reply socket");
      close(sock);
      return(-1);
    }

    for(done = 0; authtries > 0 && !done; authtries--) {
      for (config = radconfig, sreq = reqtop; sreq != NULL && !done;
	   config = config->next, sreq = sreq->next){
	req_copyto(req, sreq);

	/* send out the packet and wait for reply. */
	if (sendto(sock, (char *)&req, sreq->reqlen, 0, 
		   (struct sockaddr*) &sreq->sinr, 
		   sizeof (struct sockaddr_in)) < 0) {
          syswarn("cannot send auth_reg");
	  close(sock);
	  return(-1);
	}

	/* wait 5 seconds maximum for a radius reply. */
	now = time(0);
	end = now+5;
	tmout.tv_sec = 6;
	tmout.tv_usec = 0;
	FD_ZERO(&rdfds);
	/* store the old vector to verify next checksum */
	memcpy(secbuf+sizeof(req.vector), req.vector, sizeof(req.vector));
	FD_SET(sock, &rdfds);
	got = select(sock+1, &rdfds, 0, 0, &tmout);
	if (got < 0) {
            syswarn("cannot not select");
	    break;
	} else if (got == 0) {
	    /* timer ran out */
	    now = time(0);
	    tmout.tv_sec = end - now + 1;
	    tmout.tv_usec = 0;
	    continue;
	}
	slen = sizeof(sinl);
	if ((jlen = recvfrom(sock, (char *)&req, sizeof(req)-sizeof(int), 0, 
	                     (struct sockaddr*) &sinl, &slen)) < 0) {
            syswarn("cannot recvfrom");
	    break;
	}
	if (!config->ignore_source) {
	    if (sinl.sin_addr.s_addr != sreq->sinr.sin_addr.s_addr ||
	      (sinl.sin_port != sreq->sinr.sin_port)) {
                warn("received unexpected UDP packet from %s:%d",
                     inet_ntoa(sinl.sin_addr), ntohs(sinl.sin_port));
		continue;
	    }
	}
	sreq->reqlen = ntohs(req.length);
	if (jlen < 4+AUTH_VECTOR_LEN || jlen != sreq->reqlen) {
            warn("received badly-sized packet");
	    continue;
	}
	/* verify the checksum */
	memcpy(((char*)&req)+sreq->reqlen, config->secret, strlen(config->secret));
	memcpy(secbuf, req.vector, sizeof(req.vector));
	memcpy(req.vector, secbuf+sizeof(req.vector), sizeof(req.vector));
        md5_hash((unsigned char *)&req, strlen(config->secret)+sreq->reqlen,
	    digest);
	if (memcmp(digest, secbuf, sizeof(HASH)) != 0) {
            warn("checksum didn't match");
	    continue;
	}
	/* FINALLY!  Got back a known-good packet.  See if we're in. */
	close(sock);
	return (req.code == PW_AUTHENTICATION_ACK) ? 0 : -1;
	done = 1;
	req_copyfrom(sreq, req);
	break;
      }
    }
    if (authtries == 0)
        warn("cannot talk to remote radius server %s:%d",
             inet_ntoa(sreq->sinr.sin_addr), ntohs(sreq->sinr.sin_port));
    return(-2);
}

#define RAD_HAVE_HOST 1
#define RAD_HAVE_PORT 2
#define RAD_HAVE_PREFIX 4
#define RAD_HAVE_SUFFIX 8
#define RAD_HAVE_LOCHOST 16
#define RAD_HAVE_LOCPORT 32

int main(int argc, char *argv[])
{
    int opt;
    int havefile, haveother;
    struct auth_info *authinfo;
    rad_config_t radconfig;
    int retval;
    char *radius_config;

    message_program_name = "radius";

    if (!innconf_read(NULL))
        exit(1);

    memset(&radconfig, '\0', sizeof(rad_config_t));
    haveother = havefile = 0;

    while ((opt = getopt(argc, argv, "f:h")) != -1) {
	switch (opt) {
	  case 'f':
	    if (haveother)
                die("-f flag after another flag");
	    if (!havefile) {
              /* override the standard config completely if the user
               * specifies an alternate config file */
              memset(&radconfig, '\0', sizeof(rad_config_t));
              havefile = 1;
	    }
	    read_config(optarg, &radconfig);
	    break;
	case 'h':
	  printf("Usage: radius [-f config]\n");
          exit(0);
	}
    }
    if (argc != optind)
      exit(2);
    if (!havefile) {
      radius_config = concatpath(innconf->pathetc, INN_PATH_RADIUS_CONFIG);
      read_config(radius_config, &radconfig);
  
      free(radius_config);
    }

    authinfo = get_auth_info(stdin);
    if (authinfo == NULL)
        die("failed getting auth info");
    if (authinfo->username[0] == '\0')
        die("empty username");

    /* got username and password, check that they're valid */

    retval = rad_auth(&radconfig, authinfo->username, authinfo->password);
    if (retval == -1)
        die("user %s password doesn't match", authinfo->username);
    else if (retval == -2)
	/* couldn't talk to the radius server..  output logged above. */
	exit(1);
    else if (retval != 0)
        die("unexpected return code from authentication function: %d",
            retval);

    /* radius password matches! */
    print_user(authinfo->username);
    exit(0);
}
