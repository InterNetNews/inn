/* $Revision$
 *
 * $Id$
 *
 * radius.c - Authenticate a user against a remote radius server.
 */
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <ndbm.h>
#include <fcntl.h>
#include <stdlib.h>

#include <clibrary.h>
#include <configdata.h>
#include <macros.h>
#include <md5.h>
#include <libinn.h>
#include <nntp.h>

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

    char *prefix, *suffix;	/* futz with the username, if necessary */
} rad_config_t;

int read_config(FILE *f, rad_config_t *radconfig)
{
    char buf[SMBUF];
    char *keyword, *iter;
    int lineno;

    buf[sizeof(buf)-1] = '\0';
    lineno = 0;
    while (fgets(buf, sizeof(buf)-1, f) != (char*) 0) {
	lineno++;
	buf[strlen(buf)-1] = '\0';	/* strip '\n' */

	if (iter = strchr(buf, '#'))
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
	    if (radconfig->secret) {
		fprintf(stderr, "already found shared secret in rad_config, line %d\n",
		  lineno);
		exit(1);
	    }
	    radconfig->secret = COPY(iter);
	} else if (!strcmp(keyword, "radhost")) {
	    if (radconfig->radhost) {
		fprintf(stderr, "already found radhost in rad_config, line %d\n",
		  lineno);
		exit(1);
	    }
	    radconfig->radhost = COPY(iter);
	} else if (!strcmp(keyword, "radport")) {
	    if (radconfig->radport) {
		fprintf(stderr, "already found radport in rad_config, line %d\n",
		  lineno);
		exit(1);
	    }
	    radconfig->radport = atoi(iter);
	} else if (!strcmp(keyword, "lochost")) {
	    if (radconfig->lochost) {
		fprintf(stderr, "already found lochost in rad_config, line %d\n",
		  lineno);
		exit(1);
	    }
	    radconfig->lochost = COPY(iter);
	} else if (!strcmp(keyword, "prefix")) {
	    if (radconfig->prefix) {
		fprintf(stderr, "already found prefix in rad_config, line %d\n",
		  lineno);
		exit(1);
	    }
	    radconfig->prefix = COPY(iter);
	} else if (!strcmp(keyword, "suffix")) {
	    if (radconfig->suffix) {
		fprintf(stderr, "already found suffix in rad_config, line %d\n",
		  lineno);
		exit(1);
	    }
	    radconfig->suffix = COPY(iter);
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
#define PW_AUTHENTICATION_ACK 2
#define PW_AUTHENTICATION_REJECT 3

#define PW_USER_NAME 1
#define PW_PASSWORD 2

int alarmhit;

void alarmfunc()
{
    alarmhit = 1;
}

int rad_auth(rad_config_t *config, char *uname, char *pass)
{
    auth_req req;
    int i, j, jlen, passstart;
    char secbuf[128];
    HASH digest;
    char *sndbuf;
    struct timeval seed;
    MD5_CTX ctx;
    struct sockaddr_in sinl, sinr;
    int sock;
    struct hostent *hent;
    int done;
    int ret;
    int reqlen;

    /* seed the random number generator for the auth vector */
    gettimeofday(&seed, 0);
    srandom((unsigned) seed.tv_sec+seed.tv_usec);
    /* build the visible part of the auth vector randomly */
    for (i = 0; i < AUTH_VECTOR_LEN; i++)
	req.vector[i] = random() % 256;
    strcpy(secbuf, config->secret);
    memcpy(secbuf+strlen(config->secret), req.vector, AUTH_VECTOR_LEN);
    MD5Init(&ctx);
    MD5Update(&ctx, secbuf, strlen(config->secret)+AUTH_VECTOR_LEN);
    MD5COUNT(&ctx, strlen(config->secret)+AUTH_VECTOR_LEN);
    MD5Final(&ctx);
    memcpy(digest.hash, ctx.digest, MD5_DIGESTSIZE);
    /* fill in the auth_req data */
    req.code = PW_AUTHENTICATION_REQUEST;
    req.id = 0;

    req.data[0] = PW_USER_NAME;
    req.data[1] = strlen(uname)+2;
    strcpy(&req.data[2], uname);
    req.datalen = req.data[1];
    passstart = req.datalen;
    req.data[req.datalen] = PW_PASSWORD;
    /* Append a '\0' character to the password, to work around a really dumb
     * bug in Livingston's radius implementation. */
    req.data[req.datalen+1] = strlen(pass)+2+1;
    strcpy(&req.data[req.datalen+2], pass);
    req.datalen += req.data[req.datalen+1];

    /* filled in the data, now we know what the actual length is. */
    req.length = 4+AUTH_VECTOR_LEN+req.datalen;

    /* "encrypt" the password */
    for (i = 0; i < req.data[passstart+1]-2; i += sizeof(HASH)) {
	jlen = sizeof(HASH);
	if (req.data[passstart+1]-i-2 < sizeof(HASH))
	    jlen = req.data[passstart+1]-i-2;
	for (j = 0; j < jlen; j++)
	    req.data[passstart+2+i+j] ^= digest.hash[j];
	if (jlen == sizeof(HASH)) {
	    /* Recalculate the digest from the HASHed previous */
	    strcpy(secbuf, config->secret);
	    memcpy(secbuf+strlen(config->secret), &req.data[passstart+2+i], sizeof(HASH));
	    MD5Init(&ctx);
	    MD5Update(&ctx, secbuf, strlen(config->secret)+sizeof(HASH));
	    MD5COUNT(&ctx, strlen(config->secret)+sizeof(HASH));
	    MD5Final(&ctx);
	    memcpy(digest.hash, ctx.digest, MD5_DIGESTSIZE);
	}
    }
    reqlen = req.length;
    req.length = htons(req.length);
    /* YAYY! The auth_req is ready to go! Build the reply socket and send out
     * the message. */

    /* first, build the sockaddrs */
    bzero(&sinl, sizeof(sinl));
    sinl.sin_family = AF_INET;
    sinl.sin_len = sizeof(sinl);
    bzero(&sinr, sizeof(sinr));
    sinr.sin_family = AF_INET;
    sinr.sin_len = sizeof(sinr);
    if (config->lochost) {
	if (inet_aton(config->lochost, &sinl.sin_addr) != 1) {
	    if ((hent = gethostbyname(config->lochost)) == NULL) {
		fprintf(stderr, "radius: cant gethostbyname lochost %s\n",
		        config->lochost);
		return(-1);
	    }
	    memcpy(&sinl.sin_addr.s_addr, hent->h_addr, sizeof(struct in_addr));
	}
    }
    if (inet_aton(config->radhost, &sinr.sin_addr) != 1) {
	if ((hent = gethostbyname(config->radhost)) == NULL) {
	    fprintf(stderr, "radius: cant gethostbyname radhost %s\n",
	            config->radhost);
	    return(-1);
	}
	memcpy(&sinr.sin_addr.s_addr, hent->h_addr_list[0], sizeof(struct in_addr));
    }
    if (config->radport)
	sinr.sin_port = htons(config->radport);
    else
	sinr.sin_port = htons(PW_AUTH_UDP_PORT);
    fprintf(stderr, "Talking to remote host %s:%d\n", inet_ntoa(sinr.sin_addr),
      ntohs(sinr.sin_port));

    /* now, build the sockets */
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
	fprintf(stderr, "radius: cant build reply socket: %s\n",
	        strerror(errno));
	return(-1);
    }
    if (bind(sock, (struct sockaddr*) &sinl, sinl.sin_len) < 0) {
	fprintf(stderr, "radius: cant bind reply socket: %s\n",
	        strerror(errno));
	close(sock);
	return(-1);
    }

    /* send out the packet and wait for reply. */
    if (sendto(sock, &req, reqlen, 0, (struct sockaddr*) &sinr,
               sinr.sin_len) < 0) {
	fprintf(stderr, "radius: cant send auth_req: %s\n", strerror(errno));
	close(sock);
	return(-1);
    }
    /* wait 5 seconds maximum for a radius reply. */
    alarmhit = 0;
    signal(SIGALRM, alarmfunc);
    alarm(5);
    done = 0;
    /* store the old vector to verify next checksum */
    memcpy(secbuf+sizeof(req.vector), req.vector, sizeof(req.vector));
    while (!done && !alarmhit) {
	jlen = sizeof(sinl);
	if ((jlen = recvfrom(sock, &req, sizeof(req)-sizeof(int), 0, 
	                     (struct sockaddr*) &sinl, &jlen)) < 0) {
	    fprintf(stderr, "radius: couldnt recvfrom: %s\n", strerror(errno));
	    break;
	}
	if (memcmp(&sinl, &sinr, sizeof(sinl)) != 0) {
	    fprintf(stderr, "radius: received unexpected UDP packet.\n");
	    continue;
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
	MD5Init(&ctx);
	MD5Update(&ctx, (char*)&req, strlen(config->secret)+reqlen);
	MD5COUNT(&ctx, strlen(config->secret)+reqlen);
	MD5Final(&ctx);
	if (memcmp(ctx.digest, secbuf, sizeof(HASH)) != 0) {
	    fprintf(stderr, "radius: checksum didn't match.\n");
	    continue;
	}
	/* FINALLY!  Got back a known-good packet.  See if we're in. */
	ret = (req.code == PW_AUTHENTICATION_ACK) ? 0 : -1;
	done = 1;
    }
    alarm(0);
    signal(SIGALRM, SIG_DFL);
    close(sock);
    return(ret);
}

#define RAD_HAVE_HOST 1
#define RAD_HAVE_PORT 2
#define RAD_HAVE_PREFIX 4
#define RAD_HAVE_SUFFIX 8
#define RAD_HAVE_LOCHOST 16

int main(int argc, char *argv[])
{
    extern int optind;
    extern char *optarg;
    int opt;
    int havefile, haveother;
    char uname[SMBUF], pass[SMBUF];
    char buff[SMBUF];
    char *rpass;
    FILE *f;
    rad_config_t radconfig;

    bzero(&radconfig, sizeof(rad_config_t));
    haveother = havefile = 0;

    while ((opt = getopt(argc, argv, "f:h:p:q:s:l:")) != -1) {
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
	}
    }
    if (argc != optind)
	exit(2);
    if (!radconfig.radhost) {
	fprintf(stderr, "No radius host to authenticate against.\n");
	exit(1);
    } else if (!radconfig.secret) {
	fprintf(stderr, "No shared secret with radius host.\n");
	exit(1);
    }

    uname[0] = '\0';
    pass[0] = '\0';
    /* get the username and password from stdin */
    buff[sizeof(buff)-1] = '\0';
    while (fgets(buff, sizeof(buff)-1, stdin) != (char*) 0) {
	/* strip '\r\n' */
	buff[strlen(buff)-1] = '\0';
	if (strlen(buff) && (buff[strlen(buff)-1] == '\r'))
	    buff[strlen(buff)-1] = '\0';

#define NAMESTR "ClientAuthname: "
#define PASSSTR "ClientPassword: "
	if (!strncmp(buff, NAMESTR, strlen(NAMESTR)))
	    strncpy(uname, buff+sizeof(NAMESTR)-1, sizeof(uname));
	if (!strncmp(buff, PASSSTR, strlen(PASSSTR)))
	    strncpy(pass, buff+sizeof(PASSSTR)-1, sizeof(pass));
    }
    if (!uname[0] || !pass[0])
	exit(3);

    /* got username and password, check that they're valid */
    if (rad_auth(&radconfig, uname, pass)) {
	fprintf(stderr, "radius: user %s password doesn't match.\n", uname);
	exit(1);
    }
    /* radius password matches! */
    printf("User:%s\n", uname);
    exit(0);
}
