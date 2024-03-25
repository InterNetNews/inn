/*
 * getaddrinfo test suite.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2003-2005, 2016, 2019, 2024 Russ Allbery <eagle@eyrie.org>
 * Copyright 2015 Julien ÉLIE <julien@trigofacile.com>
 * Copyright 2007-2009, 2011-2013
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * Copying and distribution of this file, with or without modification, are
 * permitted in any medium without royalty provided the copyright notice and
 * this notice are preserved.  This file is offered as-is, without any
 * warranty.
 *
 * SPDX-License-Identifier: FSFAP
 */

#define LIBTEST_NEW_FORMAT 1

#include "config.h"
#include "portable/system.h"
#include "portable/socket.h"

#include "tap/basic.h"

/*
 * If the native platform doesn't support AI_NUMERICSERV or AI_NUMERICHOST,
 * pick some other values for them that match the values set in our
 * implementation.
 */
#if AI_NUMERICSERV == 0
#    undef AI_NUMERICSERV
#    define AI_NUMERICSERV 0x0080
#endif
#if AI_NUMERICHOST == 0
#    undef AI_NUMERICHOST
#    define AI_NUMERICHOST 0x0100
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
    struct in_addr addr, *addrs;
    struct servent *service;
    int i, result, count;
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
    saddr = (struct sockaddr_in *) (void *) ai->ai_addr;
    is_int(htons(25), saddr->sin_port, "...right port");
    is_int(htonl(0x7f000001UL), saddr->sin_addr.s_addr, "...right address");
    test_freeaddrinfo(ai);

    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_PASSIVE;
    hints.ai_socktype = SOCK_STREAM;
    ok(test_getaddrinfo(NULL, "25", &hints, &ai) == 0, "passive lookup");
    is_int(SOCK_STREAM, ai->ai_socktype, "...right socktype");
    saddr = (struct sockaddr_in *) (void *) ai->ai_addr;
    is_int(htons(25), saddr->sin_port, "...right port");
    is_int(INADDR_ANY, saddr->sin_addr.s_addr, "...right address");
    test_freeaddrinfo(ai);

    service = getservbyname("smtp", "tcp");
    if (service == NULL)
        skip_block(4, "smtp service not found");
    else {
        hints.ai_socktype = 0;
        ok(test_getaddrinfo(NULL, "smtp", &hints, &ai) == 0,
           "service of smtp");
        is_int(SOCK_STREAM, ai->ai_socktype, "...right socktype");
        saddr = (struct sockaddr_in *) (void *) ai->ai_addr;
        is_int(htons(25), saddr->sin_port, "...right port");
        is_int(INADDR_ANY, saddr->sin_addr.s_addr, "...right address");
        test_freeaddrinfo(ai);
    }

    hints.ai_flags = AI_NUMERICSERV;
    ok(test_getaddrinfo(NULL, "smtp", &hints, &ai) == EAI_NONAME,
       "AI_NUMERICSERV with smtp");
    ok(test_getaddrinfo(NULL, "25 smtp", &hints, &ai) == EAI_NONAME,
       "AI_NUMERICSERV with 25 smtp");
    ok(test_getaddrinfo(NULL, "25 ", &hints, &ai) == EAI_NONAME,
       "AI_NUMERICSERV with 25 space");
    ok(test_getaddrinfo(NULL, "25", &hints, &ai) == 0, "valid AI_NUMERICSERV");
    saddr = (struct sockaddr_in *) (void *) ai->ai_addr;
    is_int(htons(25), saddr->sin_port, "...right port");
    is_int(htonl(0x7f000001UL), saddr->sin_addr.s_addr, "...right address");
    test_freeaddrinfo(ai);

    ok(test_getaddrinfo(NULL, NULL, NULL, &ai) == EAI_NONAME, "EAI_NONAME");
    hints.ai_flags = 0x2000;
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
    saddr = (struct sockaddr_in *) (void *) ai->ai_addr;
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
        saddr = (struct sockaddr_in *) (void *) ai->ai_addr;
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
    saddr = (struct sockaddr_in *) (void *) ai->ai_addr;
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
        saddr = (struct sockaddr_in *) (void *) ai->ai_addr;
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
        saddr = (struct sockaddr_in *) (void *) ai->ai_addr;
        is_int(htons(53), saddr->sin_port, "...right port");
        ok(saddr->sin_addr.s_addr == addr.s_addr, "...right address");
        test_freeaddrinfo(ai);
    }

    /* Hopefully this will always resolve. */
    host = gethostbyname("www.eyrie.org");
    if (host == NULL)
        skip_block(9, "cannot look up www.eyrie.org");
    else {
        hints.ai_flags = 0;
        hints.ai_socktype = SOCK_STREAM;
        ok(test_getaddrinfo("www.eyrie.org", "80", &hints, &ai) == 0,
           "lookup of www.eyrie.org");
        is_int(SOCK_STREAM, ai->ai_socktype, "...right socktype");
        is_string(NULL, ai->ai_canonname, "...no canonname");
        saddr = (struct sockaddr_in *) (void *) ai->ai_addr;
        is_int(htons(80), saddr->sin_port, "...right port");
        ok(saddr->sin_addr.s_addr != INADDR_ANY, "...address is something");
        addr = saddr->sin_addr;
        test_freeaddrinfo(ai);

        hints.ai_flags = AI_CANONNAME;
        ok(test_getaddrinfo("www.eyrie.org", "80", &hints, &ai) == 0,
           "lookup of www.eyrie.org with A_CANONNAME");
        ok(ai->ai_canonname != NULL, "...canonname isn't null");
        saddr = (struct sockaddr_in *) (void *) ai->ai_addr;
        is_int(htons(80), saddr->sin_port, "...right port");
        ok(saddr->sin_addr.s_addr == addr.s_addr, "...and same address");
        test_freeaddrinfo(ai);
    }

    /* Included because it had multiple A records. */
    host = gethostbyname("cnn.com");
    if (host == NULL)
        skip_block(3, "cannot look up cnn.com");
    else {
        ai = NULL;
        ok(test_getaddrinfo("cnn.com", "80", NULL, &ai) == 0,
           "lookup of cnn.com with multiple A records");
        saddr = (struct sockaddr_in *) (void *) ai->ai_addr;
        is_int(htons(80), saddr->sin_port, "...right port");
        ok(saddr->sin_addr.s_addr != INADDR_ANY, "...address is something");
        test_freeaddrinfo(ai);
    }

    host = gethostbyname("addrinfo-test.invalid");
    if (host != NULL)
        skip("lookup of addrinfo-test.invalid succeeded");
    else {
        ai = NULL;
        result = test_getaddrinfo("addrinfo-test.invalid", NULL, NULL, &ai);
        if (result == EAI_AGAIN || result == EAI_FAIL)
            skip("lookup of invalid address returns DNS failure");
        else
            is_int(EAI_NONAME, result, "lookup of invalid address");
        if (ai != NULL)
            test_freeaddrinfo(ai);
    }

    host = gethostbyname("cnn.com");
    if (host == NULL) {
        skip_block(3, "cannot look up cnn.com");
        exit(0);
    }
    for (count = 0; host->h_addr_list[count] != NULL; count++)
        ;
    addrs = bcalloc_type(count, struct in_addr);
    for (i = 0; host->h_addr_list[i] != NULL; i++)
        memcpy(&addrs[i], host->h_addr_list[i], sizeof(addrs[i]));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_CANONNAME;
    ok(test_getaddrinfo("cnn.com", NULL, &hints, &ai) == 0,
       "lookup of cnn.com");
    saddr = (struct sockaddr_in *) (void *) ai->ai_addr;
    is_int(0, saddr->sin_port, "...port is 0");
    first = ai;
    for (found = 0; ai != NULL; ai = ai->ai_next) {
        if (strcmp(ai->ai_canonname, first->ai_canonname) != 0) {
            ok(0, "...canonname matches");
            break;
        }
        if (ai != first && ai->ai_canonname == first->ai_canonname) {
            ok(0, "...each canonname is a separate pointer");
            break;
        }
        found = 0;
        saddr = (struct sockaddr_in *) (void *) ai->ai_addr;
        addr = saddr->sin_addr;
        for (i = 0; i < count; i++)
            if (memcmp(&addr, &addrs[i], sizeof(addr)) == 0)
                found = 1;
        if (!found) {
            ok(0, "...result found in gethostbyname address list");
            break;
        }
    }
    if (found)
        ok(1, "...result found in gethostbyname address list");
    free(addrs);
    test_freeaddrinfo(first);

    return 0;
}
