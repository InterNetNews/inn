/*  $Id$
**
**  Authenticate a user against a remote radius server.
*/

#include "libauth.h"

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

#include "inn/md5.h"
#include "libinn.h"
#include "macros.h"
#include "nntp.h"
#include "paths.h"

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
} rad_config_t;

static int read_config(FILE *f, rad_config_t *radconfig)
{
    char buf[SMBUF];
    char *keyword, *iter;
    int lineno;

    buf[sizeof(buf)-1] = '\0';
    lineno = 0;
    while (fgets(buf, sizeof(buf)-1, f) != (char*) 0) {
	lineno++;
	buf[strlen(buf)-1] = '\0';	/* strip '\n' */

	if ((iter = strchr(buf, '#')) != NULL)
	    *iter = '\0';		/* strip comments */

	iter = buf+strlen(buf)-1;	/* strip trailing whitespace */
	while (iter >= buf && isspace(*iter))
	    iter--;
	iter[1] = '\0';

	if (buf[0] == '\0')
	    continue;			/* empty line */

	/* get the keyword part of the keyword: value */
	keyword = buf;
	while (isspace(*keyword))
	    keyword++;
	iter = strchr(keyword, ':');
	if (!iter) {
	    fprintf(stderr, "malformed keyword in rad_config, line %d\n", lineno);
	    exit(1);
	}
	*iter++ = '\0';

	/* now the value part */
	while (*iter && isspace(*iter))
	    iter++;
	if (!*iter) {
	    fprintf(stderr, "expecting value after keyword %s in rad_config, line %d\n",
	      keyword, lineno);
	    exit(1);
	}

        /* what are we setting? */
	if (!strcmp(keyword, "secret")) {
	    if (radconfig->secret) continue;
	    radconfig->secret = COPY(iter);
	} else if (!strcmp(keyword, "radhost")) {
	    if (radconfig->radhost) continue;
	    radconfig->radhost = COPY(iter);
	} else if (!strcmp(keyword, "radport")) {
	    if (radconfig->radport) continue;
	    radconfig->radport = atoi(iter);
	} else if (!strcmp(keyword, "lochost")) {
	    if (radconfig->lochost) continue;
	    radconfig->lochost = COPY(iter);
	} else if (!strcmp(keyword, "locport")) {
	    if (radconfig->locport) continue;
	    radconfig->locport = atoi(iter);
	} else if (!strcmp(keyword, "prefix")) {
	    if (radconfig->prefix) continue;
	    radconfig->prefix = COPY(iter);
	} else if (!strcmp(keyword, "suffix")) {
	    if (radconfig->suffix) continue;
	    radconfig->suffix = COPY(iter);
	} else if (!strcmp(keyword, "ignore-source")) {
	    if (!strcasecmp(iter, "true"))
		radconfig->ignore_source = 1;
	    else if (!strcasecmp(iter, "false"))
		radconfig->ignore_source = 0;
	    else {
		fprintf(stderr, "Expected \"true\" or \"false\" after %s in rad_config, line %d\n",
		  keyword, lineno);
		exit(1);
	    }
	} else {
	    fprintf(stderr, "unknown keyword %s in rad_config, line %d\n",
	      keyword, lineno);
	    exit(1);
	}
    }
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

static int rad_auth(rad_config_t *config, char *uname, char *pass)
{
    auth_req req;
    int i, j, jlen, passstart;
    char secbuf[128];
    HASH digest;
    struct timeval seed;
    struct sockaddr_in sinl, sinr;
    int sock;
    struct hostent *hent;
    int reqlen;
    int passlen;
    time_t now, end;
    struct timeval tmout;
    int got;
    fd_set rdfds;
    uint32_t nvalue;
    socklen_t slen;
    int authtries= 3; /* number of times to try reaching the radius server */

    /* first, build the sockaddrs */
    memset(&sinl, '\0', sizeof(sinl));
    memset(&sinr, '\0', sizeof(sinr));
    sinl.sin_family = AF_INET;
    sinr.sin_family = AF_INET;
    if (config->lochost == NULL) {
	if (gethostname(secbuf, sizeof(secbuf)) != 0) {
	    fprintf(stderr, "radius: cant get localhostname\n");
	    return(-1);
	}
	config->lochost = COPY(secbuf);
    }
    if (config->lochost) {
	if (inet_aton(config->lochost, &sinl.sin_addr) != 1) {
	    if ((hent = gethostbyname(config->lochost)) == NULL) {
		fprintf(stderr, "radius: cant gethostbyname lochost %s\n",
		        config->lochost);
		return(-1);
	    }
	    memcpy(&sinl.sin_addr.s_addr, hent->h_addr,
                   sizeof(struct in_addr));
	}
    }
    if (inet_aton(config->radhost, &sinr.sin_addr) != 1) {
	if ((hent = gethostbyname(config->radhost)) == NULL) {
	    fprintf(stderr, "radius: cant gethostbyname radhost %s\n",
	            config->radhost);
	    return(-1);
	}
	memcpy(&sinr.sin_addr.s_addr, hent->h_addr_list[0],
               sizeof(struct in_addr));
    }
    if (config->radport)
	sinr.sin_port = htons(config->radport);
    else
	sinr.sin_port = htons(PW_AUTH_UDP_PORT);

    /* seed the random number generator for the auth vector */
    gettimeofday(&seed, 0);
    srandom((unsigned) seed.tv_sec+seed.tv_usec);
    /* build the visible part of the auth vector randomly */
    for (i = 0; i < AUTH_VECTOR_LEN; i++)
	req.vector[i] = random() % 256;
    strcpy(secbuf, config->secret);
    memcpy(secbuf+strlen(config->secret), req.vector, AUTH_VECTOR_LEN);
    md5_hash(secbuf, strlen(config->secret)+AUTH_VECTOR_LEN, digest.hash);
    /* fill in the auth_req data */
    req.code = PW_AUTHENTICATION_REQUEST;
    req.id = 0;

    /* bracket the username in the configured prefix/suffix */
    req.data[0] = PW_USER_NAME;
    req.data[1] = 2;
    req.data[2] = '\0';
    if (config->prefix) {
	req.data[1] += strlen(config->prefix);
	strcat((char *)&req.data[2], config->prefix);
    }
    req.data[1] += strlen(uname);
    strcat((char *)&req.data[2], uname);
    if (!strchr(uname, '@') && config->suffix) {
	req.data[1] += strlen(config->suffix);
	strcat((char *)&req.data[2], config->suffix);
    }
    req.datalen = req.data[1];

    /* set the password */
    passstart = req.datalen;
    req.data[req.datalen] = PW_PASSWORD;
    /* Null pad the password */
    passlen = (strlen(pass) + 15) / 16;
    passlen *= 16;
    req.data[req.datalen+1] = passlen+2;
    strcpy((char *)&req.data[req.datalen+2], pass);
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
	    req.data[passstart+2+i+j] ^= digest.hash[j];
	if (jlen == sizeof(HASH)) {
	    /* Recalculate the digest from the HASHed previous */
	    strcpy(secbuf, config->secret);
	    memcpy(secbuf+strlen(config->secret), &req.data[passstart+2+i],
                   sizeof(HASH));
            md5_hash(secbuf, strlen(config->secret)+sizeof(HASH), digest.hash);
	}
    }
    reqlen = req.length;
    req.length = htons(req.length);
    /* YAYY! The auth_req is ready to go! Build the reply socket and send out
     * the message. */

    /* now, build the sockets */
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
	fprintf(stderr, "radius: cant build reply socket: %s\n",
	        strerror(errno));
	return(-1);
    }
    if (bind(sock, (struct sockaddr*) &sinl, sizeof(sinl)) < 0) {
	fprintf(stderr, "radius: cant bind reply socket: %s\n",
	        strerror(errno));
	close(sock);
	return(-1);
    }

    for( ; authtries > 0; authtries--) {
	/* send out the packet and wait for reply. */
	if (sendto(sock, (char *)&req, reqlen, 0, (struct sockaddr*) &sinr,
	       sizeof(sinr)) < 0) {
	    fprintf(stderr, "radius: cant send auth_req: %s\n", strerror(errno));
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
	    fprintf(stderr, "radius: couldn't select: %s\n", strerror(errno));
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
	    fprintf(stderr, "radius: couldnt recvfrom: %s\n", strerror(errno));
	    break;
	}
	if (!config->ignore_source) {
	    if (sinl.sin_addr.s_addr != sinr.sin_addr.s_addr ||
	      (sinl.sin_port != sinr.sin_port)) {
		fprintf(stderr, "radius: received unexpected UDP packet from %s:%d.\n",
		  inet_ntoa(sinl.sin_addr), ntohs(sinl.sin_port));
		continue;
	    }
	}
	reqlen = ntohs(req.length);
	if (jlen < 4+AUTH_VECTOR_LEN || jlen != reqlen) {
	    fprintf(stderr, "radius: received badly-sized packet.\n");
	    continue;
	}
	/* verify the checksum */
	memcpy(((char*)&req)+reqlen, config->secret, strlen(config->secret));
	memcpy(secbuf, req.vector, sizeof(req.vector));
	memcpy(req.vector, secbuf+sizeof(req.vector), sizeof(req.vector));
        md5_hash((char *)&req, strlen(config->secret)+reqlen, digest.hash);
	if (memcmp(digest.hash, secbuf, sizeof(HASH)) != 0) {
	    fprintf(stderr, "radius: checksum didn't match.\n");
	    continue;
	}
	/* FINALLY!  Got back a known-good packet.  See if we're in. */
	close(sock);
	return (req.code == PW_AUTHENTICATION_ACK) ? 0 : -1;
	break;
    }
    if (authtries == 0) {
	fprintf(stderr,
		"radius: couldn't talk to remote radius server %s:%d\n",
		inet_ntoa(sinr.sin_addr), ntohs(sinr.sin_port));
    }
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
    extern int optind;
    extern char *optarg;
    int opt;
    int havefile, haveother;
    char uname[SMBUF], pass[SMBUF];
    FILE *f;
    rad_config_t radconfig;
    int retval;
    char *radius_config;

    bzero(&radconfig, sizeof(rad_config_t));
    haveother = havefile = 0;
    if (ReadInnConf() < 0) exit(1);

    while ((opt = getopt(argc, argv, "f:h:p:P:q:s:l:S")) != -1) {
	switch (opt) {
	  case 'f':
	    if (haveother) {
		/* don't allow user to specify config file after more
		 * specific options */
		fprintf(stderr, "-f flag after another, non -f flag.\n");
		exit(1);
	    }
	    if (!havefile) {
		/* override the standard config completely if the user
		 * specifies an alternate config file */
		bzero(&radconfig, sizeof(rad_config_t));
		havefile = 1;
	    }
	    f = fopen(optarg, "r");
	    if (!f) {
		fprintf(stderr, "couldn't open config file %s: %s\n", optarg, strerror(errno));
		exit(1);
	    }
	    read_config(f, &radconfig);
	    fclose(f);
	    break;

	  case 'h':
	    if (haveother & RAD_HAVE_HOST) {
		fprintf(stderr, "two -h options.\n");
		exit(1);
	    }
	    haveother |= RAD_HAVE_HOST;
	    if (radconfig.radhost)
		DISPOSE(radconfig.radhost);
	    radconfig.radhost = optarg;
	    break;
	  case 'p':
	    if (haveother & RAD_HAVE_PORT) {
		fprintf(stderr, "two -p options.\n");
		exit(1);
	    }
	    haveother |= RAD_HAVE_PORT;
	    radconfig.radport = atoi(optarg);
	    break;
	  case 'P':
	    if (haveother & RAD_HAVE_LOCPORT) {
		fprintf(stderr, "two -P options.\n");
		exit(1);
	    }
	    haveother |= RAD_HAVE_LOCPORT;
	    radconfig.locport = atoi(optarg);
	    break;
	  case 'q':
	    if (haveother & RAD_HAVE_PREFIX) {
		fprintf(stderr, "two -q options.\n");
		exit(1);
	    }
	    haveother |= RAD_HAVE_PREFIX;
	    if (radconfig.prefix)
		DISPOSE(radconfig.prefix);
	    radconfig.prefix = optarg;
	    break;
	  case 's':
	    if (haveother & RAD_HAVE_SUFFIX) {
		fprintf(stderr, "two -s options.\n");
		exit(1);
	    }
	    haveother |= RAD_HAVE_SUFFIX;
	    if (radconfig.suffix)
		DISPOSE(radconfig.suffix);
	    radconfig.suffix = optarg;
	    break;
	  case 'l':
	    if (haveother & RAD_HAVE_LOCHOST) {
		fprintf(stderr, "two -l options.\n");
		exit(1);
	    }
	    haveother |= RAD_HAVE_LOCHOST;
	    if (radconfig.lochost)
		DISPOSE(radconfig.lochost);
	    radconfig.lochost = optarg;
	    break;
	  case 'S':
	    radconfig.ignore_source = 1;
	    break;
	}
    }
    if (argc != optind)
	exit(2);
    if (!havefile) {
	radius_config = concatpath(innconf->pathetc, _PATH_RADIUS_CONFIG);
	if (!(f = fopen(radius_config, "r"))) {
	    fprintf(stderr, "couldn't open config file %s: %s\n", radius_config,
                    strerror(errno));
	} else {
            read_config(f, &radconfig);
            fclose(f);
        }
        free(radius_config);
    }
    if (!radconfig.radhost) {
	fprintf(stderr, "No radius host to authenticate against.\n");
	exit(1);
    } else if (!radconfig.secret) {
	fprintf(stderr, "No shared secret with radius host.\n");
	exit(1);
    }

    if (get_auth(uname,pass) != 0) {
        fprintf(stderr, "radius: internal error.\n");
        exit(1);
    }

    /* got username and password, check that they're valid */
    retval = rad_auth(&radconfig, uname, pass);
    if (retval == -1) {
	fprintf(stderr, "radius: user %s password doesn't match.\n", uname);
	exit(1);
    } else if (retval == -2) {
	/* couldn't talk to the radius server..  output logged above. */
	exit(1);
    } else if (retval != 0) {
	fprintf(stderr, "radius: got unexpected return from authentication function: %d\n", retval);
	exit(1);
    }
    /* radius password matches! */
    printf("User:%s\n", uname);
    exit(0);
}
