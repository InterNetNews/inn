
/* this code was obviously taken from the Cyrus IMAP daemon source code, 
   with the appropriate copyright left intact.  There is nothing special
   about this code, so I hope nobody is going to get mad at me :) 
 */


/* login_unix_pwcheck.c -- Unix pwcheck daemon login authentication
 $Id$
 
 # Copyright 1998 Carnegie Mellon University
 # 
 # No warranties, either expressed or implied, are made regarding the
 # operation, use, or results of the software.
 #
 # Permission to use, copy, modify and distribute this software and its
 # documentation is hereby granted for non-commercial purposes only
 # provided that this copyright notice appears in all copies and in
 # supporting documentation.
 #
 # Permission is also granted to Internet Service Providers and others
 # entities to use the software for internal purposes.
 #
 # The distribution, modification or sale of a product which uses or is
 # based on the software, in whole or in part, for commercial purposes or
 # benefits requires specific, additional permission from:
 #
 #  Office of Technology Transfer
 #  Carnegie Mellon University
 #  5000 Forbes Avenue
 #  Pittsburgh, PA  15213-3890
 #  (412) 268-4387, fax: (412) 268-7395
 #  tech-transfer@andrew.cmu.edu
 *
 */
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>

#include "configdata.h"
#include "clibrary.h"
#include "nnrpd.h"

#define STATEDIR	"/var"

extern int errno;

/*
 * Unix pwcheck daemon-authenticated login (shadow password)
 */

int
login_plaintext(user, pass)
const char *user;
const char *pass;
{
    int s;
    struct sockaddr_un srvaddr;
    int r;
    struct iovec iov[10];
    static char response[1024];
    int start, n;

    s = socket(AF_UNIX, SOCK_STREAM, 0);
    if (s == -1) return errno;

    memset((char *)&srvaddr, 0, sizeof(srvaddr));
    srvaddr.sun_family = AF_UNIX;
    strcpy(srvaddr.sun_path, STATEDIR);
    strcat(srvaddr.sun_path, "/pwcheck/pwcheck");
    r = connect(s, (struct sockaddr *)&srvaddr, sizeof(srvaddr));
    if (r == -1) {
	syslog(L_NOTICE, "connect failed to pwcheck daemon - check permissions");
	return 1;
    }

    iov[0].iov_base = (char *)user;
    iov[0].iov_len = strlen(user)+1;
    iov[1].iov_base = (char *)pass;
    iov[1].iov_len = strlen(pass)+1;

    retry_writev(s, iov, 2);

    start = 0;
    while (start < sizeof(response) - 1) {
	n = read(s, response+start, sizeof(response) - 1 - start);
	if (n < 1) break;
	start += n;
    }

    close(s);

    if (start > 1 && !strncmp(response, "OK", 2)) return 0;

    response[start] = '\0';
    return 1;
}
