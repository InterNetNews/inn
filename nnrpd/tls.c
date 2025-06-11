/*
**  tls.c -- TLS functions.
**  Copyright (C) 2000 Kenichi Okada <okada@opaopa.org>.
**
**  Author: Kenichi Okada <okada@opaopa.org>
**  Created: 2000-02-22
**
**  Various bug fixes, code and documentation improvements since then
**  in 2001-2004, 2006, 2008, 2009, 2011, 2013-2019, 2021-2024.
*/

#include "portable/system.h"

#include <sys/stat.h>
#include <sys/uio.h>
#include <syslog.h>

#include "inn/innconf.h"
#include "nnrpd.h"

/* Outside the ifdef so that make depend works even ifndef HAVE_OPENSSL. */
#include "tls.h"

#ifdef HAVE_OPENSSL

#    if OPENSSL_VERSION_NUMBER < 0x010101000L
#        error "OpenSSL 1.1.1 or later is required"
#    endif

#    if defined(LIBRESSL_VERSION_NUMBER) \
        && LIBRESSL_VERSION_NUMBER < 0x02080000fL
#        error "LibreSSL 2.8.0 or later is required"
#    endif

/* We must keep some of the info available. */
static bool tls_initialized = false;

static int verify_depth;
static int do_dump = 0;
static SSL_CTX *CTX = NULL;
SSL *tls_conn = NULL;

static int tls_serverengine = 0;
static int tls_serveractive = 0; /* Available or not. */
char *tls_peer_CN = NULL;

static const char *tls_protocol = NULL;
static const char *tls_cipher_name = NULL;
static int tls_cipher_algbits = 0;
int tls_cipher_usebits = 0;

/* Set this value higher (from 1 to 4) to obtain more logs. */
static int tls_loglevel = 0;


/*
**  Taken from OpenSSL apps/lib/s_cb.c.
**  Tim -- this seems to just be giving logging messages.
*/
static void
apps_ssl_info_callback(const SSL *s, int where, int ret)
{
    const char *str;
    int w;

    if (tls_loglevel == 0)
        return;

    w = where & ~SSL_ST_MASK;

    if (w & SSL_ST_CONNECT)
        str = "SSL_connect";
    else if (w & SSL_ST_ACCEPT)
        str = "SSL_accept";
    else
        str = "undefined";

    if (where & SSL_CB_LOOP) {
        if (tls_serverengine && (tls_loglevel >= 2))
            syslog(L_NOTICE, "%s:%s", str, SSL_state_string_long(s));
    } else if (where & SSL_CB_ALERT) {
        str = (where & SSL_CB_READ) ? "read" : "write";
        if ((tls_serverengine && (tls_loglevel >= 2))
            || ((ret & 0xff) != SSL3_AD_CLOSE_NOTIFY))
            syslog(L_NOTICE, "SSL3 alert %s:%s:%s", str,
                   SSL_alert_type_string_long(ret),
                   SSL_alert_desc_string_long(ret));
    } else if (where & SSL_CB_EXIT) {
        if (ret == 0)
            syslog(L_ERROR, "%s:failed in %s", str, SSL_state_string_long(s));
        else if (ret < 0) {
            syslog(L_ERROR, "%s:error in %s", str, SSL_state_string_long(s));
        }
    }
}


/*
**  Taken from OpenSSL apps/lib/s_cb.c.
*/
static int
verify_callback(int ok, X509_STORE_CTX *ctx)
{
    char buf[256];
    X509 *err_cert;
    int err;
    int depth;

    syslog(L_NOTICE, "Doing a peer verify");

    err_cert = X509_STORE_CTX_get_current_cert(ctx);
    err = X509_STORE_CTX_get_error(ctx);
    depth = X509_STORE_CTX_get_error_depth(ctx);

    if ((tls_serveractive) && (tls_loglevel >= 1)) {
        if (err_cert != NULL) {
            X509_NAME_oneline(X509_get_subject_name(err_cert), buf,
                              sizeof(buf));
            syslog(L_NOTICE, "Peer cert verify depth=%d %s", depth, buf);
        } else {
            syslog(L_NOTICE, "Peer cert verify depth=%d <no cert>", depth);
        }
    }

    if (ok == 0) {
        syslog(L_NOTICE, "verify error:num=%d:%s", err,
               X509_verify_cert_error_string(err));

        if (verify_depth < 0 || verify_depth >= depth) {
            /* Accept the certificate in error if its depth lies within the
             * first verify_depth intermediate CA certificates, or if no
             * verification was asked. */
            ok = 1;
        } else {
            ok = 0;
        }
    }

    switch (err) {
    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
        if (err_cert != NULL) {
            X509_NAME_oneline(X509_get_issuer_name(err_cert), buf,
                              sizeof(buf));
            syslog(L_NOTICE, "issuer= %s", buf);
        } else {
            syslog(L_NOTICE, "cert has no issuer");
        }
        break;
    case X509_V_ERR_CERT_NOT_YET_VALID:
    case X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD:
        syslog(L_NOTICE, "cert not yet valid");
        break;
    case X509_V_ERR_CERT_HAS_EXPIRED:
    case X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD:
        syslog(L_NOTICE, "cert has expired");
        break;
    }
    if ((tls_serveractive) && (tls_loglevel >= 1))
        syslog(L_NOTICE, "verify return:%d", ok);

    return (ok);
}


/*
**  Taken from OpenSSL crypto/bio/bio_dump.c, modified to save a lot of strcpy
**  and strcat by Matti Aarnio.
*/
#    define DUMP_WIDTH 16

static int
tls_dump(const char *s, int len)
{
    int ret = 0;
    char buf[160 + 1];
    char *ss;
    int i;
    int j;
    int rows;
    unsigned char ch;

    rows = (len / DUMP_WIDTH);
    if ((rows * DUMP_WIDTH) < len)
        rows++;

    for (i = 0; i < rows; i++) {
        buf[0] = '\0'; /* Start with empty string. */
        ss = buf;

        snprintf(ss, sizeof(buf), "%04x ", (unsigned int) (i * DUMP_WIDTH));
        ss += strlen(ss);
        for (j = 0; j < DUMP_WIDTH; j++) {
            if (((i * DUMP_WIDTH) + j) >= len) {
                strlcpy(ss, "   ", sizeof(buf) - (ss - buf));
            } else {
                ch = ((unsigned char) *((const char *) (s) + i * DUMP_WIDTH
                                        + j))
                     & 0xff;
                snprintf(ss, sizeof(buf) - (ss - buf), "%02x%c", ch,
                         j == 7 ? '|' : ' ');
                ss += 3;
            }
        }
        ss += strlen(ss);
        *ss += ' ';
        for (j = 0; j < DUMP_WIDTH; j++) {
            if (((i * DUMP_WIDTH) + j) >= len)
                break;
            ch = ((unsigned char) *((const char *) (s) + i * DUMP_WIDTH + j))
                 & 0xff;
            *ss += (((ch >= ' ') && (ch <= '~')) ? ch : '.');
            if (j == 7)
                *ss += ' ';
        }
        *ss = 0;
        /* If this is the last call, then update the ddt_dump thing so that
         * we will move the selection point in the debug window. */
        if (tls_loglevel > 0)
            syslog(L_NOTICE, "%s", buf);
        ret += strlen(buf);
    }
    return (ret);
}


/*
**  Set up the cert things on the server side. We do need both the
**  private key (in key_file) and the cert (in cert_file).
**  Both files may be identical.
**
**  This function is taken from OpenSSL apps/lib/s_cb.c.
*/
static int
set_cert_stuff(SSL_CTX *ctx, char *cert_file, char *key_file)
{
    struct stat buf;

    if (cert_file != NULL) {
        if (SSL_CTX_use_certificate_chain_file(ctx, cert_file) <= 0) {
            syslog(L_ERROR, "unable to get certificates from '%s'", cert_file);
            return (0);
        }
        if (key_file == NULL)
            key_file = cert_file;

        /* Check ownership and permissions of key file.
         * Look at the real file (stat) and not a possible symlink (lstat). */
        if (stat(key_file, &buf) == -1) {
            syslog(L_ERROR, "unable to stat private key '%s'", key_file);
            return (0);
        }

        /* Check that the key file is a real file, isn't world-readable, and
         * that we can read it. */
        if (!S_ISREG(buf.st_mode) || (buf.st_mode & 0007) != 0
            || access(key_file, R_OK) < 0) {
            syslog(L_ERROR,
                   "bad ownership or permissions on private key"
                   " '%s': private key must be a regular file, readable by"
                   " nnrpd, and not world-readable",
                   key_file);
            return (0);
        }

        if (SSL_CTX_use_PrivateKey_file(ctx, key_file, SSL_FILETYPE_PEM)
            <= 0) {
            syslog(L_ERROR, "unable to get private key from '%s'", key_file);
            return (0);
        }
        /* Now we know that a key and cert have been set against
         * the SSL context. */
        if (!SSL_CTX_check_private_key(ctx)) {
            syslog(L_ERROR,
                   "private key does not match the certificate public key");
            return (0);
        }
    }
    return (1);
}


static const unsigned char SUPPORTED_ALP[] = {4, 'n', 'n', 't', 'p'};

static int
alpn_select_callback(SSL *ssl UNUSED, const unsigned char **out,
                     unsigned char *outlen, const unsigned char *in,
                     unsigned int inlen, void *arg UNUSED)
{
    if (SSL_select_next_proto((unsigned char **) out, outlen, SUPPORTED_ALP,
                              sizeof SUPPORTED_ALP, in, inlen)
        != OPENSSL_NPN_NEGOTIATED)
        return SSL_TLSEXT_ERR_ALERT_FATAL;

    return SSL_TLSEXT_ERR_OK;
}


/*
**  This is the setup routine for the SSL server.  As nnrpd might be called
**  more than once, we only want to do the initialization one time.
**
**  The skeleton of this function is taken from OpenSSL apps/s_server.c.
**
**  Returns -1 on error.
*/
int
tls_init_serverengine(int verifydepth, int askcert, int requirecert,
                      char *tls_CAfile, char *tls_CApath, char *tls_cert_file,
                      char *tls_key_file, bool prefer_server_ciphers,
                      bool tls_compression, struct vector *tls_proto_vect,
                      char *tls_ciphers, char *tls_ciphers13 UNUSED,
                      char *tls_ec_curve UNUSED)
{
    int off = 0;
    int verify_flags = SSL_VERIFY_NONE;
    char *CApath;
    char *CAfile;
    char *s_cert_file;
    char *s_key_file;
    size_t tls_protos = 0;
    size_t i;

    if (tls_serverengine)
        return (0); /* Already running. */

    if (tls_loglevel >= 2)
        syslog(L_NOTICE, "starting TLS engine");

    CTX = SSL_CTX_new(TLS_server_method());

    if (CTX == NULL) {
        return (-1);
    }

    off |= SSL_OP_ALL; /* Work around all known bugs. */
    SSL_CTX_set_options(CTX, off);
    SSL_CTX_set_info_callback(CTX, apps_ssl_info_callback);
    SSL_CTX_sess_set_cache_size(CTX, 128);

    if (strlen(tls_CAfile) == 0)
        CAfile = NULL;
    else
        CAfile = tls_CAfile;
    if (strlen(tls_CApath) == 0)
        CApath = NULL;
    else
        CApath = tls_CApath;

    if ((!SSL_CTX_load_verify_locations(CTX, CAfile, CApath))
        || (!SSL_CTX_set_default_verify_paths(CTX))) {
        if (tls_loglevel >= 2)
            syslog(L_ERROR, "TLS engine: cannot load CA data");
        return (-1);
    }

    if (strlen(tls_cert_file) == 0)
        s_cert_file = NULL;
    else
        s_cert_file = tls_cert_file;
    if (strlen(tls_key_file) == 0)
        s_key_file = NULL;
    else
        s_key_file = tls_key_file;

    if (!set_cert_stuff(CTX, s_cert_file, s_key_file)) {
        if (tls_loglevel >= 2)
            syslog(L_ERROR, "TLS engine: cannot load cert/key data");
        return (-1);
    }

    SSL_CTX_set_dh_auto(CTX, 1);

    /* We set a curve here by name if provided
     * or we use OpenSSL auto-selection. */
    if (tls_ec_curve != NULL) {
        if (!SSL_CTX_set1_groups_list(CTX, tls_ec_curve))
            syslog(L_ERROR, "tlseccurve '%s' not found", tls_ec_curve);
    }

    if (prefer_server_ciphers) {
        SSL_CTX_set_options(CTX, SSL_OP_CIPHER_SERVER_PREFERENCE);
    } else {
        SSL_CTX_clear_options(CTX, SSL_OP_CIPHER_SERVER_PREFERENCE);
    }

    if ((tls_proto_vect != NULL) && (tls_proto_vect->count > 0)) {
        for (i = 0; i < tls_proto_vect->count; i++) {
            if (tls_proto_vect->strings[i] != NULL) {
                if (strcmp(tls_proto_vect->strings[i], "SSLv2") == 0) {
                    syslog(
                        L_NOTICE,
                        "TLS engine: cannot enable SSLv2 - support removed");
                } else if (strcmp(tls_proto_vect->strings[i], "SSLv3") == 0) {
                    tls_protos |= INN_TLS_SSLv3;
                } else if (strcmp(tls_proto_vect->strings[i], "TLSv1") == 0) {
                    tls_protos |= INN_TLS_TLSv1;
                } else if (strcmp(tls_proto_vect->strings[i], "TLSv1.1")
                           == 0) {
                    tls_protos |= INN_TLS_TLSv1_1;
                } else if (strcmp(tls_proto_vect->strings[i], "TLSv1.2")
                           == 0) {
                    tls_protos |= INN_TLS_TLSv1_2;
                } else if (strcmp(tls_proto_vect->strings[i], "TLSv1.3")
                           == 0) {
                    tls_protos |= INN_TLS_TLSv1_3;
                } else {
                    syslog(L_ERROR,
                           "TLS engine: unknown protocol '%s' in tlsprotocols",
                           tls_proto_vect->strings[i]);
                }
            }
        }
    } else {
        /* Default value: allow only secure TLS protocols. */
        tls_protos = (INN_TLS_TLSv1_2 | INN_TLS_TLSv1_3);
    }

    if ((tls_protos & INN_TLS_SSLv3) == 0) {
        SSL_CTX_set_options(CTX, SSL_OP_NO_SSLv3);
    }

    if ((tls_protos & INN_TLS_TLSv1) == 0) {
        SSL_CTX_set_options(CTX, SSL_OP_NO_TLSv1);
    }

    if ((tls_protos & INN_TLS_TLSv1_1) == 0) {
        SSL_CTX_set_options(CTX, SSL_OP_NO_TLSv1_1);
    }

    if ((tls_protos & INN_TLS_TLSv1_2) == 0) {
        SSL_CTX_set_options(CTX, SSL_OP_NO_TLSv1_2);
    }

    if ((tls_protos & INN_TLS_TLSv1_3) == 0) {
#    ifdef SSL_OP_NO_TLSv1_3
        SSL_CTX_set_options(CTX, SSL_OP_NO_TLSv1_3);
#    endif
    }

    if (tls_ciphers != NULL) {
        if (SSL_CTX_set_cipher_list(CTX, tls_ciphers) == 0) {
            syslog(L_ERROR, "TLS engine: cannot set cipher list");
            return (-1);
        }
    }

#    if !defined(LIBRESSL_VERSION_NUMBER) \
        || LIBRESSL_VERSION_NUMBER >= 0x03040100fL
    /* In LibreSSL, SSL_CTX_set_ciphersuites was introduced in version 3.4.1.
     */
    if (tls_ciphers13 != NULL) {
        if (SSL_CTX_set_ciphersuites(CTX, tls_ciphers13) == 0) {
            syslog(L_ERROR, "TLS engine: cannot set ciphersuites");
            return (-1);
        }
    }
#    endif

    if (tls_compression) {
        SSL_CTX_clear_options(CTX, SSL_OP_NO_COMPRESSION);
    } else {
        SSL_CTX_set_options(CTX, SSL_OP_NO_COMPRESSION);
    }

    verify_depth = verifydepth;
    /* Options for OPT_VERIFY in OpenSSL apps/s_server.c. */
    if (askcert != 0)
        verify_flags |= SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE;
    /* Options for OPT_UPPER_V_VERIFY in OpenSSL apps/s_server.c. */
    if (requirecert)
        verify_flags |= SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT
                        | SSL_VERIFY_CLIENT_ONCE;
    SSL_CTX_set_verify(CTX, verify_flags, verify_callback);

    SSL_CTX_set_client_CA_list(CTX, SSL_load_client_CA_file(CAfile));

    SSL_CTX_set_alpn_select_cb(CTX, alpn_select_callback, NULL);

    tls_serverengine = 1;
    return (0);
}


/*
**  The function called by nnrpd to initialize the TLS support.  Calls
**  tls_init_serverengine and checks the result.  On any sort of failure,
**  nnrpd will exit.
**
**  Returns -1 on error.
*/
int
tls_init(void)
{
    int ssl_result;

    if (tls_initialized)
        return 0;

    ssl_result = tls_init_serverengine(
        5, /* Depth to verify. */
        0, /* Can client auth? */
        0, /* Required client to auth? */
        innconf->tlscafile, innconf->tlscapath, innconf->tlscertfile,
        innconf->tlskeyfile, innconf->tlspreferserverciphers,
        innconf->tlscompression, innconf->tlsprotocols, innconf->tlsciphers,
        innconf->tlsciphers13, innconf->tlseccurve);

    if (ssl_result == -1) {
        Reply("%d Error initializing TLS\r\n",
              initialSSL ? NNTP_FAIL_TERMINATING : NNTP_ERR_STARTTLS);
        syslog(L_ERROR,
               "error initializing TLS: "
               "[CA_file: %s] [CA_path: %s] [cert_file: %s] [key_file: %s]",
               innconf->tlscafile, innconf->tlscapath, innconf->tlscertfile,
               innconf->tlskeyfile);
        if (initialSSL)
            ExitWithStats(1, false);
        return -1;
    }

    tls_initialized = true;
    return 0;
}


/*
**  Taken from OpenSSL apps/lib/s_cb.c.
**
**  The prototype of the callback function changed with BIO_set_callback_ex()
**  introduced in OpenSSL 1.1.1 and LibreSSL 3.5.0.
*/
#    if defined(LIBRESSL_VERSION_NUMBER) \
        && LIBRESSL_VERSION_NUMBER < 0x030500000L
static long
bio_dump_cb(BIO *bio, int cmd, const char *argp, int argi, long argl UNUSED,
            long ret)
{
    if (!do_dump)
        return (ret);

    if (cmd == (BIO_CB_READ | BIO_CB_RETURN)) {
        syslog(L_NOTICE, "read from %p [%p] (%d bytes => %ld (0x%lX))",
               (void *) bio, (void *) argp, argi, ret, (unsigned long) ret);
        tls_dump(argp, (int) ret);
        return (ret);
    } else if (cmd == (BIO_CB_WRITE | BIO_CB_RETURN)) {
        syslog(L_NOTICE, "write to %p [%p] (%d bytes => %ld (0x%lX))",
               (void *) bio, (void *) argp, argi, ret, (unsigned long) ret);
        tls_dump(argp, (int) ret);
    }
    return (ret);
}
#    else
static long
bio_dump_cb(BIO *bio, int cmd, const char *argp, size_t len, int argi UNUSED,
            long argl UNUSED, int ret, size_t *processed)
{
    if (!do_dump)
        return (ret);

    if (cmd == (BIO_CB_READ | BIO_CB_RETURN)) {
        if (ret > 0 && processed != NULL) {
            syslog(L_NOTICE, "read from %p [%p] (%lu bytes => %lu (0x%lX))",
                   (void *) bio, (void *) argp, (unsigned long) len,
                   (unsigned long) *processed, (unsigned long) *processed);
            tls_dump(argp, (int) *processed);
        } else {
            syslog(L_NOTICE, "read from %p [%p] (%lu bytes => %d (0x%lX))",
                   (void *) bio, (void *) argp, (unsigned long) len, ret,
                   (unsigned long) ret);
        }
    } else if (cmd == (BIO_CB_WRITE | BIO_CB_RETURN)) {
        if (ret > 0 && processed != NULL) {
            syslog(L_NOTICE, "write to %p [%p] (%lu bytes => %lu (0x%lX))",
                   (void *) bio, (void *) argp, (unsigned long) len,
                   (unsigned long) *processed, (unsigned long) *processed);
            tls_dump(argp, (int) *processed);
        } else {
            syslog(L_NOTICE, "write to %p [%p] (%lu bytes => %d (0x%lX))",
                   (void *) bio, (void *) argp, (unsigned long) len, ret,
                   (unsigned long) ret);
        }
    }
    return (ret);
}
#    endif


/*
**  This is the actual startup routine for the connection.  We expect
**  that the buffers are flushed and the "382 Continue with TLS negotiation"
**  was sent to the client (if using STARTTLS), so that we can immediately
**  start the TLS handshake process.
**
**  layerbits and authid are filled in on success; authid is only
**  filled in if the client authenticated.
*/
int
tls_start_servertls(int readfd, int writefd)
{
    int sts;
    int keepalive;
    SSL_SESSION *session;
    SSL_CIPHER *cipher;

    if (!tls_serverengine) {
        /* It should never happen. */
        syslog(L_ERROR, "tls_engine not running");
        return (-1);
    }
    if (tls_loglevel >= 1)
        syslog(L_NOTICE, "setting up TLS connection");

    if (tls_conn == NULL) {
        tls_conn = (SSL *) SSL_new(CTX);
    }
    if (tls_conn == NULL) {
        return (-1);
    }
    SSL_clear(tls_conn);

#    if defined(SOL_SOCKET) && defined(SO_KEEPALIVE)
    /* Set KEEPALIVE to catch broken socket connections. */
    keepalive = 1;
    if (setsockopt(readfd, SOL_SOCKET, SO_KEEPALIVE, &keepalive,
                   sizeof(keepalive))
        < 0)
        syslog(L_ERROR, "fd %d can't setsockopt(KEEPALIVE) %m", readfd);
    if (setsockopt(writefd, SOL_SOCKET, SO_KEEPALIVE, &keepalive,
                   sizeof(keepalive))
        < 0)
        syslog(L_ERROR, "fd %d can't setsockopt(KEEPALIVE) %m", writefd);
#    endif /* SOL_SOCKET && SO_KEEPALIVE */

    /* Set the file descriptors for SSL to use. */
    if (SSL_set_rfd(tls_conn, readfd) == 0) {
        return (-1);
    }

    if (SSL_set_wfd(tls_conn, writefd) == 0) {
        return (-1);
    }

    /* This is the actual handshake routine.  It will do all the negotiations
     * and will check the client cert etc. */
    SSL_set_accept_state(tls_conn);

    /* We do have an SSL_set_fd() and now suddenly a BIO_ routine is called?
     * Well there is a BIO below the SSL routines that is automatically
     * created for us, so we can use it for debugging purposes. */
    if (tls_loglevel >= 3) {
        /* In LibreSSL, BIO_set_callback_ex was introduced in version 3.5.0. */
#    if defined(LIBRESSL_VERSION_NUMBER) \
        && LIBRESSL_VERSION_NUMBER < 0x030500000L
        BIO_set_callback(SSL_get_rbio(tls_conn), bio_dump_cb);
#    else
        BIO_set_callback_ex(SSL_get_rbio(tls_conn), bio_dump_cb);
#    endif
    }

    /* Dump the negotiation for loglevels 3 and 4. */
    if (tls_loglevel >= 3)
        do_dump = 1;

    if ((sts = SSL_accept(tls_conn)) <= 0) {
        session = SSL_get_session(tls_conn);

        if (session) {
            SSL_CTX_remove_session(CTX, session);
        }
        if (tls_conn)
            SSL_free(tls_conn);
        tls_conn = NULL;
        return (-1);
    }
    /* Only loglevel 4 dumps everything. */
    if (tls_loglevel < 4)
        do_dump = 0;

    tls_protocol = SSL_get_version(tls_conn);
    cipher = (SSL_CIPHER *) SSL_get_current_cipher(tls_conn);

    tls_cipher_name = SSL_CIPHER_get_name(cipher);
    tls_cipher_usebits = SSL_CIPHER_get_bits(cipher, &tls_cipher_algbits);
    tls_serveractive = 1;

    syslog(
        L_NOTICE, "starttls: %s with cipher %s (%d/%d bits) no authentication",
        tls_protocol, tls_cipher_name, tls_cipher_usebits, tls_cipher_algbits);

    return (0);
}


ssize_t
SSL_writev(SSL *ssl, const struct iovec *vector, int count)
{
    static char *buffer = NULL;
    static size_t allocsize = 0;
    char *bp;
    size_t bytes, to_copy;
    int i;
    /* Find the total number of bytes to be written. */
    bytes = 0;
    for (i = 0; i < count; ++i)
        bytes += vector[i].iov_len;
    /* Allocate a buffer to hold the data. */
    if (NULL == buffer) {
        size_t to_alloc = (bytes > 0 ? bytes : 1);
        buffer = (char *) xmalloc(to_alloc);
        allocsize = to_alloc;
    } else if (bytes > allocsize) {
        buffer = (char *) xrealloc(buffer, bytes);
        allocsize = bytes;
    }
    /* Copy the data into BUFFER. */
    to_copy = bytes;
    bp = buffer;
    for (i = 0; i < count; ++i) {
#    define min(a, b) ((a) > (b) ? (b) : (a))
        size_t copy = min(vector[i].iov_len, to_copy);
        memcpy(bp, vector[i].iov_base, copy);
        bp += copy;
        to_copy -= copy;
        if (to_copy == 0)
            break;
    }
    return SSL_write(ssl, buffer, bytes);
}

#endif /* HAVE_OPENSSL */
