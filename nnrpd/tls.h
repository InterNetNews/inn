/*
**  tls.h -- TLS functions.
**  Copyright (C) 2000 Kenichi Okada <okada@opaopa.org>.
**
**  Author: Kenichi Okada <okada@opaopa.org>
**  Created: 2000-02-22
*/

#ifndef TLS_H
#define TLS_H 1

#ifdef HAVE_OPENSSL

#    include <openssl/bn.h>
#    include <openssl/dh.h>
#    include <openssl/err.h>
#    include <openssl/lhash.h>
#    include <openssl/pem.h>
#    include <openssl/rand.h>
#    include <openssl/ssl.h>
#    include <openssl/x509.h>

#    if !defined(OPENSSL_NO_EC) && defined(TLSEXT_ECPOINTFORMAT_uncompressed)
#        include <openssl/ec.h>
#        include <openssl/objects.h>
#        define HAVE_OPENSSL_ECC
#    endif

/* Protocol support. */
#    define INN_TLS_SSLv2   1
#    define INN_TLS_SSLv3   2
#    define INN_TLS_TLSv1   4
#    define INN_TLS_TLSv1_1 8
#    define INN_TLS_TLSv1_2 16
#    define INN_TLS_TLSv1_3 32


extern SSL *tls_conn;
extern int tls_cipher_usebits;
extern char *tls_peer_CN;

/* Init TLS engine. */
int tls_init_serverengine(int verifydepth, /* Depth to verify. */
                          int askcert,     /* 1 = Verify client. */
                          int requirecert, /* 1 = Another client verify? */
                          char *tls_CAfile, char *tls_CApath,
                          char *tls_cert_file, char *tls_key_file,
                          bool prefer_server_ciphers, bool tls_compression,
                          struct vector *tls_protocols, char *tls_ciphers,
                          char *tls_ciphers13, char *tls_ec_curve);

/* Init TLS. */
int tls_init(void);

/* Start TLS negotiation. */
int tls_start_servertls(int readfd, int writefd);

ssize_t SSL_writev(SSL *ssl, const struct iovec *vector, int count);

#endif /* HAVE_OPENSSL */

#endif /* TLS_H */
