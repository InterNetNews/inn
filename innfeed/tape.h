/*  $Id$
**
**  The public interface to the Tape class.
**
**  Written by James Brister <brister@vix.com>
**  Copyright 1996 by the Internet Software Consortium
**
**  For license terms, see the end of this file.
**
**  The Tape class simulates a mag tape.  It only reads or writes Articles.  A
**  tape is either in an Input or Output state.  When an Article is given to a
**  Tape it will store the Article in memory until it reaches a highwater mark
**  at which point it dumps all it's articles to disk.
**
**  Input tapes generate article objects on request if the underlying tape
**  file has info in it.  The Tapes take care of cleaning up used-up files as
**  needed.
*/

#if ! defined ( tape_h__ )
#define tape_h__

#include <stdio.h>

#include "misc.h"


/* If dontRotate is true, then any articles that get written to the tape
   will never be read back in again. This is for the batch-mode-only case
   where articles written to tape were done so 'cause the remote
   temporarily rejected them. */
Tape newTape (const char *peerName, bool dontRotate) ;

void gPrintTapeInfo (FILE *fp, unsigned int inedntAmt) ;
void printTapeInfo (Tape tape, FILE *fp, unsigned int indentAmt) ;

/* deletes the tape objects. If it has any articles cached then it dumps
   them to the disk. */
void delTape (Tape tape) ;

/* give an article to the Tape for storage */
void tapeTakeArticle (Tape tape, Article article) ;

/* get a new article from an Input tape. */
Article getArticle (Tape tape) ;

/* close the input and output files and reopen them. */
void gFlushTapes (void) ;
void tapeFlush (Tape tape) ;


/**************************************************/
/*               CLASS LEVEL FUNCTIONS            */
/**************************************************/

/* get all the active input tapes to checkpoint their current positions */
void checkPointTapes (void) ;

/* get the name of the directory tapes are being stored in. */
const char *getTapeDirectory (void) ;

/* set the size limit of the output tapes. Default is zero which is no
   limit. */
void setOutputSizeLimit (long limit) ;

int tapeConfigLoadCbk (void *data) ;

void tapeLogGlobalStatus (FILE *fp) ;
void tapeLogStatus (Tape tape, FILE *fp) ;

#endif /* tape_h__ */

/*
**  Copyright 1996 by the Internet Software Consortium
**
**  Permission to use, copy, modify, and distribute this software for any
**  purpose with or without fee is hereby granted, provided that the above
**  copyright notice and this permission notice appear in all copies.
**
**  THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
**  DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
**  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL
**  INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT,
**  OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
**  USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
**  OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
**  PERFORMANCE OF THIS SOFTWARE.
*/
