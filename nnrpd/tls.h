/* tls.h --- TLSv1 functions
   Copyright (C) 2000 Kenichi Okada <okada@opaopa.org>

   Author: Kenichi Okada <okada@opaopa.org>
   Created: 2000-02-22

   Keywords: TLS, OpenSSL

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.
   If not, write to the Free Software Foundation,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

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
