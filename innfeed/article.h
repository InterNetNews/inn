/* -*- c -*-
 *
 * Author:      James Brister <brister@vix.com> -- berkeley-unix --
 * Start Date:  Wed Dec 27 10:09:28 1995
 * Project:     INN (innfeed)
 * File:        article.h
 * RCSId:       $Id$
 * Copyright:   Copyright (c) 1996 by Internet Software Consortium
 *
 *              Permission to use, copy, modify, and distribute this
 *              software for any purpose with or without fee is hereby
 *              granted, provided that the above copyright notice and this
 *              permission notice appear in all copies.
 *
 *              THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE
 *              CONSORTIUM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 *              SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *              MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET
 *              SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 *              INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *              WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 *              WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 *              TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE
 *              USE OR PERFORMANCE OF THIS SOFTWARE.
 * Description: The public interface to articles. The articles are
 *              implemented via reference counting. This interface
 *              provides the methods for getting the contents of the article.
 *
 *              When an Article is created there's a chance that another
 *              copy of it already exists. For example if the Article is
 *              pulled out of a Tape for a particular host it may already
 *              be in existance in some other host. This class will manage
 *              this situation to prevent multiple copies of the article
 *              being in core.
 */

#if ! defined ( article_h__ )
#define article_h__

#include <stdio.h>

#include "misc.h"


  /* Create a new Article object. FILENAME is the path of the file the */
  /* article is in. MSGID is the news message id of the article */
Article newArticle (const char *filename, const char *msgid) ;

  /* delete the given article. Just decrements refcount and then FREEs if the
     refcount is 0. */
void delArticle (Article article) ;

void gPrintArticleInfo (FILE *fp, unsigned int indentAmt) ;

  /* print some debugging info about the article. */
void printArticleInfo (Article art, FILE *fp, unsigned int indentAmt) ;

  /* return true if this article's file still exists. */
bool artFileIsValid (Article article) ;

  /* return true if we have the article's contents (calling this may trigger
     the reading off the disk). */
bool artContentsOk (Article article) ;

  /* increments reference count and returns a copy of article that can be
     kept (or passed off to someone else) */
Article artTakeRef (Article article) ;

  /* return the pathname of the file the article is in. */
const char *artFileName (Article article) ;

  /* return a list of buffers suitable for giving to an endpoint. The return
     value can (must) be given to freeBufferArray */
Buffer *artGetNntpBuffers (Article article) ;

  /* return the message id stoed in the article object */
const char *artMsgId (Article article) ;

  /* return size of the article */
int artSize (Article article) ;

  /* return the number of buffers that artGetNntpBuffers() would return. */
unsigned int artNntpBufferCount (Article article) ;

  /* tell the Article class to log (or not) missing articles as they occur. */
void artLogMissingArticles (bool val) ;

  /* if VAL is true, then when an article is read off disk the necesary
     carriage returns are inserted instead of setting up iovec-style buffers
     for writev. Useful for systems like solaris that have very small max
     number of iovecs that writev can take. Must be called only once before
     the first article is created.  */
void artBitFiddleContents (bool val) ;

  /* set the limit on the number of bytes in all articles (this is not a hard
     limit). Can only be called one time before any articles are created. */
void artSetMaxBytesInUse (unsigned int val) ;

#endif /* article_h__ */
