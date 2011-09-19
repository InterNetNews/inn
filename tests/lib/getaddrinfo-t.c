/*
 * getaddrinfo test suite.
 * 
 * $Id$
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <rra@stanford.edu>
 *
 * The authors hereby relinquish any claim to any copyright that they may have
 * in this work, whether granted under contract or by operation of law or
 * international treaty, and hereby commit to the public, at large, that they
 * shall not, at any time in the future, seek to enforce any copyright in this
 * work against any person or entity, or prevent any person or entity from
 * copying, publishing, distributing or creating derivative works of this
 * work.
 */

#define LIBTEST_NEW_FORMAT 1

#include "config.h"
#include "clibrary.h"
#include "portable/socket.h"

#include "inn/messages.h"
#include "libtest.h"

/*
 * If the native platform doesn't support AI_NUMERICSERV or AI_NUMERICHOST,
 * pick some other values for them that match the values set in our
 * implementation.
 */
#if AI_NUMERICSERV == 0
# undef AI_NUMERICSERV
# define AI_NUMERICSERV 0x0080
#endif
#if AI_NUMERICHOST == 0
# undef AI_NUMERICHOST
# define AI_NUMERICHOST 0x0100
#endif

const char *test_gai_strerror(int);
void test_freeaddrinfo(struct addrinfo *);
int test_getaddrinfo(const char *, const char *, const struct addrinfo *,
                     struct addrinfo **);

int
main(void)
{
    struct addrinfo *ai, *first;
    struct addrinfo hints;
    struct sockaddr_in *saddr;
    struct hostent *host;
    struct in_addr addr;
    struct servent *service;
    int i, result;
    int found;

    plan(75);

    is_string("Host name lookup failure", test_gai_strerror(1),
              "gai_strerror(1)");
    is_string("System error", test_gai_strerror(9), "gai_strerror(9)");
    is_string("Unknown error", test_gai_strerror(40), "gai_strerror(40)");
    is_string("Unknown error", test_gai_strerror(-37), "gai_strerror(-37)");

    ok(test_getaddrinfo(NULL, "25", NULL, &ai) == 0, "service of 25");
    is_int(AF_INET, ai->ai_family, "...right family");
    is_int(0, ai->ai_socktype, "...right socktype");
    is_int(IPPROTO_TCP, ai->ai_protocol, "...right protocol");
    is_string(NULL, ai->ai_canonname, "...no canonname");
    is_int(sizeof(struct sockaddr_in), ai->ai_addrlen, "...right addrlen");
    saddr = (struct sockaddr_in *) ai->ai_addr;
    is_int(htons(25), saddr->sin_port, "...right port");
    ok(saddr->sin_addr.s_addr == htonl(0x7f000001UL), "...right address");
    test_freeaddrinfo(ai);

    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_PASSIVE;
    hints.ai_socktype = SOCK_STREAM;
    ok(test_getaddrinfo(NULL, "25", &hints, &ai) == 0, "passive lookup");
    is_int(SOCK_STREAM, ai->ai_socktype, "...right socktype");
    saddr = (struct sockaddr_in *) ai->ai_addr;
    is_int(htons(25), saddr->sin_port, "...right port");
    ok(saddr->sin_addr.s_addr == INADDR_ANY, "...right address");
    test_freeaddrinfo(ai);

    service = getservbyname("smtp", "tcp");
    if (service == NULL)
        skip_block(4, "smtp service not found");
    else {
        hints.ai_socktype = 0;
        ok(test_getaddrinfo(NULL, "smtp", &hints, &ai) == 0,
           "service of smtp");
        is_int(SOCK_STREAM, ai->ai_socktype, "...right socktype");
        saddr = (struct sockaddr_in *) ai->ai_addr;
        is_int(htons(25), saddr->sin_port, "...right port");
        ok(saddr->sin_addr.s_addr == INADDR_ANY, "...right address");
        test_freeaddrinfo(ai);
    }

    hints.ai_flags = AI_NUMERICSERV;
    ok(test_getaddrinfo(NULL, "smtp", &hints, &ai) == EAI_NONAME,
       "AI_NUMERICSERV with smtp");
    ok(test_getaddrinfo(NULL, "25 smtp", &hints, &ai) == EAI_NONAME,
       "AI_NUMERICSERV with 25 smtp");
    ok(test_getaddrinfo(NULL, "25 ", &hints, &ai) == EAI_NONAME,
       "AI_NUMERICSERV with 25 space");
    ok(test_getaddrinfo(NULL, "25", &hints, &ai) == 0,
       "valid AI_NUMERICSERV");
    saddr = (struct sockaddr_in *) ai->ai_addr;
    is_int(htons(25), saddr->sin_port, "...right port");
    ok(saddr->sin_addr.s_addr == htonl(0x7f000001UL), "...right address");
    test_freeaddrinfo(ai);

    ok(test_getaddrinfo(NULL, NULL, NULL, &ai) == EAI_NONAME, "EAI_NONAME");
    hints.ai_flags = 2000;
    ok(test_getaddrinfo(NULL, "25", &hints, &ai) == EAI_BADFLAGS,
       "EAI_BADFLAGS");
    hints.ai_flags = 0;
    hints.ai_socktype = SOCK_RAW;
    ok(test_getaddrinfo(NULL, "25", &hints, &ai) == EAI_SOCKTYPE,
       "EAI_SOCKTYPE");
    hints.ai_socktype = 0;
    hints.ai_family = AF_UNIX;
    ok(test_getaddrinfo(NULL, "25", &hints, &ai) == EAI_FAMILY, "EAI_FAMILY");
    hints.ai_family = AF_UNSPEC;

    inet_aton("10.20.30.40", &addr);
    ok(test_getaddrinfo("10.20.30.40", NULL, NULL, &ai) == 0,
       "IP address lookup");
    is_int(AF_INET, ai->ai_family, "...right family");
    is_int(0, ai->ai_socktype, "...right socktype");
    is_int(IPPROTO_TCP, ai->ai_protocol, "...right protocol");
    is_string(NULL, ai->ai_canonname, "...no canonname");
    is_int(sizeof(struct sockaddr_in), ai->ai_addrlen, "...right addrlen");
    saddr = (struct sockaddr_in *) ai->ai_addr;
    is_int(0, saddr->sin_port, "...right port");
    ok(saddr->sin_addr.s_addr == addr.s_addr, "...right address");
    test_freeaddrinfo(ai);

    if (service == NULL)
        skip_block(7, "smtp service not found");
    else {
        ok(test_getaddrinfo("10.20.30.40", "smtp", &hints, &ai) == 0,
           "IP address lookup with smtp service");
        is_int(AF_INET, ai->ai_family, "...right family");
        is_int(SOCK_STREAM, ai->ai_socktype, "...right socktype");
        is_int(IPPROTO_TCP, ai->ai_protocol, "...right protocol");
        is_string(NULL, ai->ai_canonname, "...no canonname");
        saddr = (struct sockaddr_in *) ai->ai_addr;
        is_int(htons(25), saddr->sin_port, "...right port");
        ok(saddr->sin_addr.s_addr == addr.s_addr, "...right address");
        test_freeaddrinfo(ai);
    }

    hints.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV;
    ok(test_getaddrinfo("10.2.3.4", "smtp", &hints, &ai) == EAI_NONAME,
       "AI_NUMERICHOST and AI_NUMERICSERV with symbolic service");
    ok(test_getaddrinfo("example.com", "25", &hints, &ai) == EAI_NONAME,
       "AI_NUMERICHOST and AI_NUMERICSERV with symbolic name");
    ok(test_getaddrinfo("10.20.30.40", "25", &hints, &ai) == 0,
       "valid AI_NUMERICHOST and AI_NUMERICSERV");
    saddr = (struct sockaddr_in *) ai->ai_addr;
    is_int(htons(25), saddr->sin_port, "...right port");
    ok(saddr->sin_addr.s_addr == addr.s_addr, "...right address");
    test_freeaddrinfo(ai);

    if (service == NULL)
        skip_block(4, "smtp service not found");
    else {
        hints.ai_flags = AI_NUMERICHOST | AI_CANONNAME;
        ok(test_getaddrinfo("10.20.30.40", "smtp", &hints, &ai) == 0,
           "AI_NUMERICHOST and AI_CANONNAME");
        is_string("10.20.30.40", ai->ai_canonname, "...right canonname");
        saddr = (struct sockaddr_in *) ai->ai_addr;
        is_int(htons(25), saddr->sin_port, "...right port");
        ok(saddr->sin_addr.s_addr == addr.s_addr, "...right address");
        test_freeaddrinfo(ai);
    }

    service = getservbyname("domain", "udp");
    if (service == NULL)
        skip_block(5, "domain service not found");
    else {
        hints.ai_flags = 0;
        hints.ai_socktype = SOCK_DGRAM;
        ok(test_getaddrinfo("10.20.30.40", "domain", &hints, &ai) == 0,
           "domain service with UDP hint");
        is_int(SOCK_DGRAM, ai->ai_socktype, "...right socktype");
        is_string(NULL, ai->ai_canonname, "...no canonname");
        saddr = (struct sockaddr_in *) ai->ai_addr;
        is_int(htons(53), saddr->sin_port, "...right port");
        ok(saddr->sin_addr.s_addr == addr.s_addr, "...right address");
        test_freeaddrinfo(ai);
    }

    /* Hopefully this will always resolve. */
    host = gethostbyname("www.isc.org");
    if (host == NULL)
        skip_block(9, "cannot look up www.isc.org");
    else {
        hints.ai_flags = 0;
        hints.ai_socktype = SOCK_STREAM;
        ok(test_getaddrinfo("www.isc.org", "80", &hints, &ai) == 0,
           "lookup of www.isc.org");
        is_int(SOCK_STREAM, ai->ai_socktype, "...right socktype");
        is_string(NULL, ai->ai_canonname, "...no canonname");
        saddr = (struct sockaddr_in *) ai->ai_addr;
        is_int(htons(80), saddr->sin_port, "...right port");
        ok(saddr->sin_addr.s_addr != INADDR_ANY, "...address is something");
        addr = saddr->sin_addr;
        test_freeaddrinfo(ai);

        hints.ai_flags = AI_CANONNAME;
        ok(test_getaddrinfo("www.isc.org", "80", &hints, &ai) == 0,
           "lookup of www.isc.org with A_CANONNAME");
        ok(ai->ai_canonname != NULL, "...canonname isn't null");
        saddr = (struct sockaddr_in *) ai->ai_addr;
        is_int(htons(80), saddr->sin_port, "...right port");
        ok(saddr->sin_addr.s_addr == addr.s_addr, "...and same address");
        test_freeaddrinfo(ai);
    }

    /* Included because it had multiple A records. */
    host = gethostbyname("cnn.com");
    if (host == NULL)
        skip_block(3, "cannot look up cnn.com");
    else {
        ok(test_getaddrinfo("cnn.com", "80", NULL, &ai) == 0,
           "lookup of cnn.com with multiple A records");
        saddr = (struct sockaddr_in *) ai->ai_addr;
        is_int(htons(80), saddr->sin_port, "...right port");
        ok(saddr->sin_addr.s_addr != INADDR_ANY, "...address is something");
        test_freeaddrinfo(ai);
    }

    host = gethostbyname("addrinfo-test.invalid");
    if (host != NULL)
        skip("lookup of addrinfo-test.invalid succeeded");
    else {
        result = test_getaddrinfo("addrinfo-test.invalid", NULL, NULL, &ai);
        is_int(EAI_NONAME, result, "lookup of invalid address");
    }

    host = gethostbyname("cnn.com");
    if (host == NULL) {
        skip_block(3, "cannot look up cnn.com");
        exit(0);
    }
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_CANONNAME;
    ok(test_getaddrinfo("cnn.com", NULL, &hints, &ai) == 0,
       "lookup of cnn.com");
    saddr = (struct sockaddr_in *) ai->ai_addr;
    is_int(0, saddr->sin_port, "...port is 0");
    first = ai;
    for (found = 0; ai != NULL; ai = ai->ai_next) {
        if (!strcmp(ai->ai_canonname, first->ai_canonname) == 0) {
            ok(0, "...canonname matches");
            break;
        }
        if (ai != first && ai->ai_canonname == first->ai_canonname) {
            ok(0, "...each canonname is a separate pointer");
            break;
        }
        found = 0;
        saddr = (struct sockaddr_in *) ai->ai_addr;
        addr = saddr->sin_addr;
        for (i = 0; host->h_addr_list[i] != NULL; i++)
            if (memcmp(&addr, host->h_addr_list[i], host->h_length) == 0)
                found = 1;
        if (!found) {
            ok(0, "...result found in gethostbyname address list");
            break;
        }
    }
    if (found)
        ok(1, "...result found in gethostbyname address list");
    test_freeaddrinfo(ai);

    return 0;
}
