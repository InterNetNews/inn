#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "macros.h"

int main(int argc, char *argv[])
{
    char buf[2048];
    char *domain;

    if (argc != 2) {
	fprintf(stderr, "Usage:\n\t%s <domain>\n", argv[0]);
	exit(1);
    }
    /* read the connection info from stdin */
    domain = 0;
#define CLIENTHOST "ClientHost: "
    while(fgets(buf, sizeof(buf), stdin) != (char*) 0) {
	/* strip '\n' */
	buf[strlen(buf)-1] = '\0';
	if (buf[strlen(buf)-1] == '\r')
	    buf[strlen(buf)-1] = '\0';

	if (!strncmp(buf, CLIENTHOST, strlen(CLIENTHOST)))
	    domain = COPY(buf+strlen(CLIENTHOST));
    }

    if (!domain) {
	fprintf(stderr, "domain: didn't get clienthost.\n");
	exit(1);
    }
    if (strlen(domain) < strlen(argv[1]) ||
      (strcmp(domain+strlen(domain)-strlen(argv[1]), argv[1]))) {
	fprintf(stderr, "domain: domain %s didn't match %s\n", domain, argv[1]);
	exit(1);
    }
    *(domain+strlen(domain)-strlen(argv[1])) = '\0';
    printf("User:%s\n", domain);

    return(0);
}
