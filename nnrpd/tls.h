/* tls.h --- TLSv1 functions
   Copyright (C) 2000 Kenichi Okada <okada@opaopa.org>

   Author: Kenichi Okada <okada@opaopa.org>
   Created: 2000-02-22

   Keywords: TLS, OpenSSL

   Commentary:

   [RFC 2246] "The TLS Protocol Version 1.0"
        by Christopher Allen <callen@certicom.com> and
        Tim Dierks <tdierks@certicom.com> (1999/01)

   [RFC 2595] "Using TLS with IMAP, POP3 and ACAP"
        by Chris Newman <chris.newman@innosoft.com> (1999/06)

*/

#ifdef HAVE_SSL

#ifndef TLS_H
#define TLS_H

#include <openssl/lhash.h>
#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/ssl.h>

/* init tls */
int tls_init_serverengine(int verifydepth, /* depth to verify */
			  int askcert,     /* 1 = verify client */
			  int requirecert, /* 1 = another client verify? */
			  char *tls_CAfile,
			  char *tls_CApath,
			  char *tls_cert_file,
			  char *tls_key_file);

/* start tls negotiation */
int tls_start_servertls(int readfd, int writefd);

ssize_t SSL_writev (SSL *ssl, const struct iovec *vector, int count);

#endif /* CYRUSTLS_H */

#endif /* HAVE_SSL */
