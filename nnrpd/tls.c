/* tls.c --- TLSv1 functions
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

#include <sys/types.h>
#include "config.h"
#include "nnrpd.h"

#ifdef HAVE_SSL

/* System library. */

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/uio.h>

/* taken from lib/parsedate.c */
#ifndef WRITEV_USE_ALLOCA
#ifdef alloca
#define WRITEV_USE_ALLOCA
#else /* alloca not defined */
#ifdef __GNUC__
#define WRITEV_USE_ALLOCA
#define alloca __builtin_alloca
#else /* not GNU C.  */
#if (!defined (__STDC__) && defined (sparc)) || defined (__sparc__) || defined (__sparc) || defined (__sgi) || (defined (__sun) && defined (__i386))
#define WRITEV_USE_ALLOCA
#include <alloca.h>
#else /* not sparc */
#if (defined (_MSDOS) || defined (_MSDOS_)) && !defined (__TURBOC__)
#else /* not MSDOS, or __TURBOC__ */
#if defined(_AIX)
 #pragma alloca
#define WRITEV_USE_ALLOCA
#endif /* not _AIX */
#endif /* not MSDOS, or __TURBOC__ */
#endif /* not sparc */
#endif /* not GNU C */
#endif /* alloca not defined */
#endif /* WRITEV_USE_ALLOCA not defined */
#ifdef WRITEV_USE_ALLOCA
#define WRITEV_ALLOC alloca
#else
#define WRITEV_ALLOC malloc
#endif


#endif

/* outside the ifdef so `make depend` works even ifndef HAVE_SSL */
#include "tls.h"

#ifdef HAVE_SSL

/* We must keep some of the info available */
static const char hexcodes[] = "0123456789ABCDEF";

static int verify_depth;
static int verify_error = X509_V_OK;
static int do_dump = 0;
static SSL_CTX *CTX = NULL;
SSL *tls_conn = NULL;

#define CCERT_BUFSIZ 256

int     tls_serverengine = 0;
int     tls_serveractive = 0;	/* available or not */
char   *tls_peer_subject = NULL;
char   *tls_peer_issuer = NULL;
char   *tls_peer_fingerprint = NULL;

int     tls_clientactive = 0;	/* available or not */
char   *tls_peer_CN = NULL;
char   *tls_issuer_CN = NULL;

const char   *tls_protocol = NULL;
const char   *tls_cipher_name = NULL;
int	tls_cipher_usebits = 0;
int	tls_cipher_algbits = 0;


int tls_loglevel = 0;


/* taken from OpenSSL apps/s_cb.c 
 * tim - this seems to just be giving logging messages
 */

static void apps_ssl_info_callback(SSL * s, int where, int ret)
{
    const char  *str;
    int         w;

    if (tls_loglevel==0) return;

    w = where & ~SSL_ST_MASK;

    if (w & SSL_ST_CONNECT)
	str = "SSL_connect";
    else if (w & SSL_ST_ACCEPT)
	str = "SSL_accept";
    else
	str = "undefined";

    if (where & SSL_CB_LOOP) {
	if (tls_serverengine && (tls_loglevel >= 2))
	    Printf("%s:%s", str, SSL_state_string_long(s));
    } else if (where & SSL_CB_ALERT) {
	str = (where & SSL_CB_READ) ? "read" : "write";
	if ((tls_serverengine && (tls_loglevel >= 2)) ||
	    ((ret & 0xff) != SSL3_AD_CLOSE_NOTIFY))
	  Printf("SSL3 alert %s:%s:%s", str,
		 SSL_alert_type_string_long(ret),
		 SSL_alert_desc_string_long(ret));
    } else if (where & SSL_CB_EXIT) {
	if (ret == 0)
	    Printf("%s:failed in %s",
		     str, SSL_state_string_long(s));
	else if (ret < 0) {
	    Printf("%s:error in %s",
		     str, SSL_state_string_long(s));
	}
    }
}

/* taken from OpenSSL apps/s_cb.c */

static RSA *tmp_rsa_cb(SSL * s UNUSED, int export UNUSED, int keylength)
{
    static RSA *rsa_tmp = NULL;

    if (rsa_tmp == NULL) {
	rsa_tmp = RSA_generate_key(keylength, RSA_F4, NULL, NULL);
    }
    return (rsa_tmp);
}

/* taken from OpenSSL apps/s_cb.c */

static int verify_callback(int ok, X509_STORE_CTX * ctx)
{
    char    buf[256];
    X509   *err_cert;
    int     err;
    int     depth;

    syslog(L_NOTICE,"Doing a peer verify");

    err_cert = X509_STORE_CTX_get_current_cert(ctx);
    err = X509_STORE_CTX_get_error(ctx);
    depth = X509_STORE_CTX_get_error_depth(ctx);

    X509_NAME_oneline(X509_get_subject_name(err_cert), buf, 256);
    if ((tls_serveractive) && (tls_loglevel >= 1))
      Printf("Peer cert verify depth=%d %s", depth, buf);
    if (ok==0)
    {
      syslog(L_NOTICE, "verify error:num=%d:%s", err,
	     X509_verify_cert_error_string(err));
      
	if (verify_depth >= depth) {
	    ok = 0;
	    verify_error = X509_V_OK;
	} else {
	    ok = 0;
	    verify_error = X509_V_ERR_CERT_CHAIN_TOO_LONG;
	}
    }
    switch (ctx->error) {
    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
	X509_NAME_oneline(X509_get_issuer_name(ctx->current_cert), buf, 256);
	syslog(L_NOTICE, "issuer= %s", buf);
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
      Printf("verify return:%d", ok);

    return (ok);
}


/*
 * taken from OpenSSL crypto/bio/b_dump.c, modified to save a lot of strcpy
 * and strcat by Matti Aarnio.
 */

#define TRUNCATE
#define DUMP_WIDTH	16

static int tls_dump(const char *s, int len)
{
    int     ret = 0;
    char    buf[160 + 1];
    char    *ss;
    int     i;
    int     j;
    int     rows;
    int     trunc;
    unsigned char ch;

    trunc = 0;


#ifdef TRUNCATE
    for (; (len > 0) && ((s[len - 1] == ' ') || (s[len - 1] == '\0')); len--)
	trunc++;
#endif

    rows = (len / DUMP_WIDTH);
    if ((rows * DUMP_WIDTH) < len)
	rows++;

    for (i = 0; i < rows; i++) {
	buf[0] = '\0';				/* start with empty string */
	ss = buf;

	sprintf(ss, "%04x ", i * DUMP_WIDTH);
	ss += strlen(ss);
	for (j = 0; j < DUMP_WIDTH; j++) {
	    if (((i * DUMP_WIDTH) + j) >= len) {
		strcpy(ss, "   ");
	    } else {
		ch = ((unsigned char) *((const char *)(s) + i * DUMP_WIDTH + j))
		    & 0xff;
		sprintf(ss, "%02x%c", ch, j == 7 ? '|' : ' ');
		ss += 3;
	    }
	}
	ss += strlen(ss);
	*ss+= ' ';
	for (j = 0; j < DUMP_WIDTH; j++) {
	    if (((i * DUMP_WIDTH) + j) >= len)
		break;
	    ch = ((unsigned char) *((const char *)(s) + i * DUMP_WIDTH + j))
		& 0xff;
	    *ss+= (((ch >= ' ') && (ch <= '~')) ? ch : '.');
	    if (j == 7) *ss+= ' ';
	}
	*ss = 0;
	/* 
	 * if this is the last call then update the ddt_dump thing so that
         * we will move the selection point in the debug window
         */	
	if (tls_loglevel>0)
	  Printf("%s", buf);
	ret += strlen(buf);
    }
#ifdef TRUNCATE
    if (trunc > 0) {
	snprintf(buf, sizeof(buf), "%04x - <SPACES/NULS>\n", len+ trunc);
	if (tls_loglevel>0)
	  Printf("%s", buf);
	ret += strlen(buf);
    }
#endif
    return (ret);
}

 /*
  * Set up the cert things on the server side. We do need both the
  * private key (in key_file) and the cert (in cert_file).
  * Both files may be identical.
  *
  * This function is taken from OpenSSL apps/s_cb.c
  */

static int set_cert_stuff(SSL_CTX * ctx, char *cert_file, char *key_file)
{
	struct stat buf;

    if (cert_file != NULL) {
	if (SSL_CTX_use_certificate_file(ctx, cert_file,
					 SSL_FILETYPE_PEM) <= 0) {
	    syslog(L_ERROR, "unable to get certificate from '%s'", cert_file);
	    return (0);
	}
	if (key_file == NULL)
	    key_file = cert_file;

	/* check ownership and permissions of key file */
	if (lstat(key_file, &buf) == -1) {
	    syslog(L_ERROR, "unable to stat private key '%s'", cert_file);
	    return (0);
	}
	if (!S_ISREG(buf.st_mode) || (buf.st_mode & 0077) != 0 ||
	    buf.st_uid != getuid()) {
	    syslog(L_ERROR, "bad ownership or permissions on private key '%s'", 
		  cert_file);
	    return (0);
	}

	if (SSL_CTX_use_PrivateKey_file(ctx, key_file,
					SSL_FILETYPE_PEM) <= 0) {
	    syslog(L_ERROR, "unable to get private key from '%s'", key_file);
	    return (0);
	}
	/* Now we know that a key and cert have been set against
         * the SSL context */
	if (!SSL_CTX_check_private_key(ctx)) {
	    syslog(L_ERROR, "Private key does not match the certificate public key");
	    return (0);
	}
    }
    return (1);
}



 /*
  * This is the setup routine for the SSL server. As smtpd might be called
  * more than once, we only want to do the initialization one time.
  *
  * The skeleton of this function is taken from OpenSSL apps/s_server.c.

  * returns -1 on error
  */

int tls_init_serverengine(int verifydepth,
			  int askcert,
			  int requirecert,
			  char *tls_CAfile,
			  char *tls_CApath,
			  char *tls_cert_file,
			  char *tls_key_file
			  )
{
    int     off = 0;
    int     verify_flags = SSL_VERIFY_NONE;
    char   *CApath;
    char   *CAfile;
    char   *s_cert_file;
    char   *s_key_file;

    if (tls_serverengine)
      return (0);				/* already running */

    if (tls_loglevel >= 2)
      Printf("starting TLS engine");

    SSL_load_error_strings();
    SSLeay_add_ssl_algorithms();

    CTX = SSL_CTX_new(SSLv23_server_method());
    if (CTX == NULL) {
      return (-1);
    };

    off |= SSL_OP_ALL;		/* Work around all known bugs */
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

    if ((!SSL_CTX_load_verify_locations(CTX, CAfile, CApath)) ||
	(!SSL_CTX_set_default_verify_paths(CTX))) {
      if (tls_loglevel >= 2)
	Printf("TLS engine: cannot load CA data\n");
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
	Printf("TLS engine: cannot load cert/key data\n");
      return (-1);
    }
    SSL_CTX_set_tmp_rsa_callback(CTX, tmp_rsa_cb);

    verify_depth = verifydepth;
    if (askcert!=0)
	verify_flags |= SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE;
    if (requirecert)
	verify_flags |= SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT
	    | SSL_VERIFY_CLIENT_ONCE;
    SSL_CTX_set_verify(CTX, verify_flags, verify_callback);

    SSL_CTX_set_client_CA_list(CTX, SSL_load_client_CA_file(CAfile));

    tls_serverengine = 1;
    return (0);
}


/* taken from OpenSSL apps/s_cb.c */

static long bio_dump_cb(BIO * bio, int cmd, const char *argp, int argi,
			long argl UNUSED, long ret)
{
    if (!do_dump)
	return (ret);

    if (cmd == (BIO_CB_READ | BIO_CB_RETURN)) {
	Printf("read from %08X [%08lX] (%d bytes => %ld (0x%X))", (unsigned int) bio, (long unsigned int) argp,
		 argi, ret, (unsigned int) ret);
	tls_dump(argp, (int) ret);
	return (ret);
    } else if (cmd == (BIO_CB_WRITE | BIO_CB_RETURN)) {
	Printf("write to %08X [%08lX] (%d bytes => %ld (0x%X))", (unsigned int) bio, (long unsigned int)argp,
		 argi, ret, (unsigned int) ret);
	tls_dump(argp, (int) ret);
    }
    return (ret);
}

 /*
  * This is the actual startup routine for the connection. We expect
  * that the buffers are flushed and the "220 Ready to start TLS" was
  * send to the client, so that we can immediately can start the TLS
  * handshake process.
  *
  * layerbits and authid are filled in on sucess. authid is only
  * filled in if the client authenticated
  * 
  */
int tls_start_servertls(int readfd, int writefd)
{
    int     sts;
    SSL_SESSION *session;
    SSL_CIPHER *cipher;

    if (!tls_serverengine)
    {		
      /* should never happen */
      syslog(L_ERROR, "tls_engine not running");
      return (-1);
    }
    if (tls_loglevel >= 1)
	Printf("setting up TLS connection");

    if (tls_conn == NULL)
    {
	tls_conn = (SSL *) SSL_new(CTX);
    }
    if (tls_conn == NULL)
    {
	return (-1);
    }
    SSL_clear(tls_conn);

    
    /* set the file descriptors for SSL to use */
    if (SSL_set_rfd(tls_conn, readfd)==0)
    {
      return (-1);
    }

    if (SSL_set_wfd(tls_conn, writefd)==0)
    {
      return (-1);
    }
    
    /*
     * This is the actual handshake routine. It will do all the negotiations
     * and will check the client cert etc.
     */
    SSL_set_accept_state(tls_conn);

    /*
     * We do have an SSL_set_fd() and now suddenly a BIO_ routine is called?
     * Well there is a BIO below the SSL routines that is automatically
     * created for us, so we can use it for debugging purposes.
     */
    if (tls_loglevel >= 3)
	BIO_set_callback(SSL_get_rbio(tls_conn), bio_dump_cb);

    /* Dump the negotiation for loglevels 3 and 4*/
    if (tls_loglevel >= 3)
	do_dump = 1;

      if ((sts = SSL_accept(tls_conn)) <= 0) { /* xxx <= 0 */
	session = SSL_get_session(tls_conn);

	if (session) {
	  SSL_CTX_remove_session(CTX, session);
	}
	if (tls_conn)
	  SSL_free(tls_conn);
	tls_conn = NULL;
	return (-1);
      }
      /* Only loglevel==4 dumps everything */
      if (tls_loglevel < 4)
	do_dump = 0;

    tls_protocol = SSL_get_version(tls_conn);
    cipher = SSL_get_current_cipher(tls_conn);

    tls_cipher_name = SSL_CIPHER_get_name(cipher);
    tls_cipher_usebits = SSL_CIPHER_get_bits(cipher,
						 &tls_cipher_algbits);
    tls_serveractive = 1;

    syslog(L_NOTICE, "starttls: %s with cipher %s (%d/%d bits) no authentication", tls_protocol, tls_cipher_name,
	   tls_cipher_usebits, tls_cipher_algbits);

    return (0);
}

ssize_t
SSL_writev (ssl, vector, count)
     SSL *ssl;
     const struct iovec *vector;
     int count;
{
  char *buffer;
  register char *bp;
  size_t bytes, to_copy;
  int i;
  /* Find the total number of bytes to be written.  */
  bytes = 0;
  for (i = 0; i < count; ++i)
    bytes += vector[i].iov_len;
  /* Allocate a temporary buffer to hold the data.  */
  buffer = (char *) WRITEV_ALLOC (bytes);
  /* Copy the data into BUFFER.  */
  to_copy = bytes;
  bp = buffer;
  for (i = 0; i < count; ++i)
    {
#define min(a, b)       ((a) > (b) ? (b) : (a))
      size_t copy = min (vector[i].iov_len, to_copy);
      (void) memcpy ((void *) bp, (void *) vector[i].iov_base, copy);
      bp += copy;
      to_copy -= copy;
      if (to_copy == 0)
        break;
    }
  return SSL_write (ssl, buffer, bytes);
}


#endif /* HAVE_SSL */
