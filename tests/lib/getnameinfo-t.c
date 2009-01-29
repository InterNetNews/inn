/* $Id$ */
/* getnameinfo test suite. */

#include "config.h"
#include "clibrary.h"
#include "portable/socket.h"

#include "inn/messages.h"
#include "inn/libinn.h"
#include "libtest.h"

int test_getnameinfo(const struct sockaddr *, socklen_t, char *, socklen_t,
                     char *, socklen_t, int);

/* Linux doesn't provide EAI_OVERFLOW, so make up our own for testing. */
#ifndef EAI_OVERFLOW
# define EAI_OVERFLOW 10
#endif

int
main(void)
{
    char node[256], service[256];
    struct sockaddr_in sin;
    struct sockaddr *sa = (struct sockaddr *) &sin;
    int status;
    struct hostent *hp;
    char *name;

    test_init(26);

    /* Test the easy stuff that requires no assumptions.  Hopefully everyone
       has nntp, exec, and biff as services. */
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(119);
    inet_aton("10.20.30.40", &sin.sin_addr);
    status = test_getnameinfo(sa, sizeof(sin), NULL, 0, NULL, 0, 0);
    ok_int(1, EAI_NONAME, status);
    status = test_getnameinfo(sa, sizeof(sin), node, 0, service, 0, 0);
    ok_int(2, EAI_NONAME, status);
    status = test_getnameinfo(sa, sizeof(sin), NULL, 0, service,
                              sizeof(service), 0);
    ok_int(3, 0, status);
    ok_string(4, "nntp", service);
    service[0] = '\0';
    status = test_getnameinfo(sa, sizeof(sin), NULL, 0, service, 1, 0);
    ok_int(5, EAI_OVERFLOW, status);
    status = test_getnameinfo(sa, sizeof(sin), NULL, 0, service, 4, 0);
    ok_int(6, EAI_OVERFLOW, status);
    status = test_getnameinfo(sa, sizeof(sin), NULL, 0, service, 5, 0);
    ok_int(7, 0, status);
    ok_string(8, "nntp", service);
    status = test_getnameinfo(sa, sizeof(sin), NULL, 0, service,
                              sizeof(service), NI_NUMERICSERV);
    ok_int(9, 0, status);
    ok_string(10, "119", service);
    sin.sin_port = htons(512);
    status = test_getnameinfo(sa, sizeof(sin), NULL, 0, service,
                              sizeof(service), 0);
    ok_int(11, 0, status);
    ok_string(12, "exec", service);
    status = test_getnameinfo(sa, sizeof(sin), NULL, 0, service,
                              sizeof(service), NI_DGRAM);
    ok_int(13, 0, status);
    ok_string(14, "biff", service);
    status = test_getnameinfo(sa, sizeof(sin), node, sizeof(node), NULL, 0,
                              NI_NUMERICHOST);
    ok_int(15, 0, status);
    ok_string(16, "10.20.30.40", node);
    node[0] = '\0';
    status = test_getnameinfo(sa, sizeof(sin), node, 1, NULL, 0,
                              NI_NUMERICHOST);
    ok_int(17, EAI_OVERFLOW, status);
    status = test_getnameinfo(sa, sizeof(sin), node, 11, NULL, 0,
                              NI_NUMERICHOST);
    ok_int(18, EAI_OVERFLOW, status);
    status = test_getnameinfo(sa, sizeof(sin), node, 12, NULL, 0,
                              NI_NUMERICHOST);
    ok_int(19, 0, status);
    ok_string(20, "10.20.30.40", node);

    /* Okay, now it gets annoying.  Do a forward and then reverse lookup of
       some well-known host and make sure that getnameinfo returns the same
       results.  This may need to be skipped. */
    hp = gethostbyname("www.isc.org");
    if (hp == NULL)
        skip_block(21, 2, "cannot look up www.isc.org");
    else {
        memcpy(&sin.sin_addr, hp->h_addr, sizeof(sin.sin_addr));
        hp = gethostbyaddr((char *) &sin.sin_addr, sizeof(sin.sin_addr), AF_INET);
        if (hp == NULL || strchr(hp->h_name, '.') == NULL)
            skip_block(21, 2, "cannot reverse-lookup www.isc.org");
        else {
            name = xstrdup(hp->h_name);
            status = test_getnameinfo(sa, sizeof(sin), node, sizeof(node),
                                      NULL, 0, 0);
            ok_int(21, 0, status);
            ok_string(22, name, node);
            free(name);
        }
    }

    /* Hope that no one is weird enough to put 0.0.0.0 into DNS. */
    inet_aton("0.0.0.0", &sin.sin_addr);
    status = test_getnameinfo(sa, sizeof(sin), node, sizeof(node), NULL, 0, 0);
    ok_int(23, 0, status);
    ok_string(24, "0.0.0.0", node);
    node[0] = '\0';
    status = test_getnameinfo(sa, sizeof(sin), node, sizeof(node), NULL, 0,
                              NI_NAMEREQD);
    ok_int(25, EAI_NONAME, status);

    sin.sin_family = AF_UNIX;
    status = test_getnameinfo(sa, sizeof(sin), node, sizeof(node), NULL, 0, 0);
    ok_int(26, EAI_FAMILY, status);

    return 0;
}
