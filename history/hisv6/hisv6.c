/*  $Id$
**
**  History v6 implementation against the history API.
**
**  Copyright (c) 2001, Thus plc 
**  
**  Redistribution and use of the source code in source and binary 
**  forms, with or without modification, are permitted provided that
**  the following 3 conditions are met:
**  
**  1. Redistributions of the source code must retain the above 
**  copyright notice, this list of conditions and the disclaimer 
**  set out below. 
**  
**  2. Redistributions of the source code in binary form must 
**  reproduce the above copyright notice, this list of conditions 
**  and the disclaimer set out below in the documentation and/or 
**  other materials provided with the distribution. 
**  
**  3. Neither the name of the Thus plc nor the names of its 
**  contributors may be used to endorse or promote products 
**  derived from this software without specific prior written 
**  permission from Thus plc. 
**  
**  Disclaimer:
**  
**  "THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
**  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
**  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
**  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE DIRECTORS
**  OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
**  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
**  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
**  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
**  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
**  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
**  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
*/

#include "config.h"
#include "clibrary.h"
#include <fcntl.h>
#include <limits.h>
#include <errno.h>
#include "hisinterface.h"
#include "hisv6.h"
#include "hisv6-private.h"
#include "dbz.h"
#include "inn/innconf.h"
#include "inn/timer.h"
#include "inn/qio.h"
#include "inn/sequence.h"
#include "inndcomm.h"

/*
**  because we can only have one open dbz per process, we keep a
**  pointer to which of the current history structures owns it
*/
static struct hisv6 *hisv6_dbzowner;


/*
**  set error status to that indicated by s; doesn't copy the string,
**  assumes the caller did that for us
*/
static void
hisv6_seterror(struct hisv6 *h, const char *s)
{
    his_seterror(h->history, s);
}


/*
**  format line or offset into a string for error reporting
*/
static void
hisv6_errloc(char *s, size_t line, off_t offset)
{
    if (offset != -1) {
	/* really we want an autoconf test for %ll/%L/%I64, sigh */
	sprintf(s, "@%.0f", (double)offset);
    } else {
	sprintf(s, ":%lu", (unsigned long)line);
    }
}


/*
**  split a history line into its constituent components; return a
**  bitmap indicating which components we're returning are valid (or
**  would be valid if a NULL pointer is passed for that component) or
**  -1 for error.  *error is set to a string which describes the
**  failure.
*/
static int
hisv6_splitline(const char *line, const char **error, HASH *hash,
		 time_t *arrived, time_t *posted, time_t *expires,
		 TOKEN *token)
{
    char *p = (char *)line;
    unsigned long l;
    int r = 0;

    /* parse the [...] hash field */
    if (*p != '[') {
	*error = "`[' missing from history line";
	return -1;
    }
    ++p;
    if (hash)
	*hash = TextToHash(p);
    p += 32;
    if (*p != ']') {
	*error = "`]' missing from history line";
	return -1;
    }
    ++p;
    r |= HISV6_HAVE_HASH;
    if (*p != HISV6_FIELDSEP) {
	*error = "field separator missing from history line";
	return -1;
    }

    /* parse the arrived field */
    l = strtoul(p + 1, &p, 10);
    if (l == ULONG_MAX) {
	*error = "arrived timestamp out of range";
	return -1;
    }
    r |= HISV6_HAVE_ARRIVED;
    if (arrived)
	*arrived = (time_t)l;
    if (*p != HISV6_SUBFIELDSEP) {
	/* no expires or posted time */
	if (posted)
	    *posted = 0;
	if (expires)
	    *expires = 0;
    } else {
	/* parse out the expires field */
	++p;
	if (*p == HISV6_NOEXP) {
	    ++p;
	    if (expires)
		*expires = 0;
	} else {
	    l = strtoul(p, &p, 10);
	    if (l == ULONG_MAX) {
		*error = "expires timestamp out of range";
		return -1;
	    }
	    r |= HISV6_HAVE_EXPIRES;
	    if (expires)
		*expires = (time_t)l;
	}
	/* parse out the posted field */
	if (*p != HISV6_SUBFIELDSEP) {
	    /* no posted time */
	    if (posted)
		*posted = 0;
	} else {
	    ++p;
	    l = strtoul(p, &p, 10);
	    if (l == ULONG_MAX) {
		*error = "posted timestamp out of range";
		return -1;
	    }
	    r |= HISV6_HAVE_POSTED;
	    if (posted)
		*posted = (time_t)l;
	}
    }

    /* parse the token */
    if (*p == HISV6_FIELDSEP)
	++p;
    else if (*p != '\0') {
	*error = "field separator missing from history line";
	return -1;
    }
    /* IsToken false would imply a remembered line, or where someone's
     * used prunehistory */
    if (IsToken(p)) {
	r |= HISV6_HAVE_TOKEN;
	if (token)
	    *token = TextToToken(p);
    }
    return r;
}


/*
**  Given the time, now, return the time at which we should next check
**  the history file
*/
static unsigned long
hisv6_nextcheck(struct hisv6 *h, unsigned long now)
{
    return now + h->statinterval;
}


/*
**  close any dbz structures associated with h; we also manage the
**  single dbz instance voodoo
*/
static bool
hisv6_dbzclose(struct hisv6 *h)
{
    bool r = true;

    if (h == hisv6_dbzowner) {
	if (!hisv6_sync(h))
	    r = false;
	if (!dbzclose()) {
	    hisv6_seterror(h, concat("can't dbzclose ",
				      h->histpath, " ",
				      strerror(errno), NULL));
	    r = false;
	}
	hisv6_dbzowner = NULL;
    }
    return r;
}


/*
**  close an existing history structure, cleaning it to the point
**  where we can reopon without leaking resources
*/
static bool
hisv6_closefiles(struct hisv6 *h)
{
    bool r = true;

    if (!hisv6_dbzclose(h))
	r = false;

    if (h->readfd != -1) {
	if (close(h->readfd) != 0 && errno != EINTR) {
	    hisv6_seterror(h, concat("can't close history ",
				      h->histpath, " ",
				      strerror(errno),NULL));
	    r = false;
	}
	h->readfd = -1;
    }

    if (h->writefp != NULL) {
	if (ferror(h->writefp) || fflush(h->writefp) == EOF) {
	    hisv6_seterror(h, concat("error on history ",
				      h->histpath, " ",
				      strerror(errno), NULL));
	    r = false;
	}
	if (Fclose(h->writefp) == EOF) {
	    hisv6_seterror(h, concat("can't fclose history ",
				      h->histpath, " ",
				      strerror(errno), NULL));
	    r = false;
	}
	h->writefp = NULL;
        h->offset = 0;
    }

    h->nextcheck = 0;
    h->st.st_ino = (ino_t)-1;
    h->st.st_dev = (dev_t)-1;
    return r;
}


/*
**  Reopen (or open from fresh) a history structure; assumes the flags
**  & path are all set up, ready to roll. If we don't own the dbz, we
**  suppress the dbz code; this is needed during expiry (since the dbz
**  code doesn't yet understand multiple open contexts... yes its a
**  hack)
*/
static bool
hisv6_reopen(struct hisv6 *h)
{
    bool r = false;

    if (h->flags & HIS_RDWR) {
	const char *mode;

	if (h->flags & HIS_CREAT)
	    mode = "w";
	else
	    mode = "a";
	if ((h->writefp = Fopen(h->histpath, mode, INND_HISTORY)) == NULL) {
	    hisv6_seterror(h, concat("can't fopen history ",
				      h->histpath, " ",
				      strerror(errno), NULL));
	    hisv6_closefiles(h);
	    goto fail;
	}

	/* fseeko to the end of file because the result of ftello() is
	   undefined for files freopen()-ed in append mode according
	   to POSIX 1003.1.  ftello() is used later on to determine a
	   new article's offset in the history file. Fopen() uses
	   freopen() internally. */
	if (fseeko(h->writefp, 0, SEEK_END) == -1) {
	    hisv6_seterror(h, concat("can't fseek to end of ",
				      h->histpath, " ",
				      strerror(errno), NULL));
	    hisv6_closefiles(h);
	    goto fail;
	}
        h->offset = ftello(h->writefp);
	if (h->offset == -1) {
	    hisv6_seterror(h, concat("can't ftello ", h->histpath, " ",
				      strerror(errno), NULL));
	    hisv6_closefiles(h);
	    goto fail;
	}
	close_on_exec(fileno(h->writefp), true);
    }

    /* Open the history file for reading. */
    if ((h->readfd = open(h->histpath, O_RDONLY)) < 0) {
	hisv6_seterror(h, concat("can't open ", h->histpath, " ",
				  strerror(errno), NULL));
	hisv6_closefiles(h);
	goto fail;
    }
    close_on_exec(h->readfd, true);
    
    /* if there's no current dbz owner, claim it here */
    if (hisv6_dbzowner == NULL) {
	hisv6_dbzowner = h;
    }

    /* During expiry we need two history structures in place, so we
       have to select which one gets the dbz file */
    if (h == hisv6_dbzowner) {
	dbzoptions opt;

	/* Open the DBZ file. */
	dbzgetoptions(&opt);

	/* HIS_INCORE usually means we're rebuilding from scratch, so
	   keep the whole lot in core until we flush */
	if (h->flags & HIS_INCORE) {
	    opt.writethrough = false;
	    opt.pag_incore = INCORE_MEM;
#ifndef	DO_TAGGED_HASH
	    opt.exists_incore = INCORE_MEM;
#endif
	} else {
	    opt.writethrough = true;
#ifdef	DO_TAGGED_HASH
	    opt.pag_incore = INCORE_MMAP;
#else
	    /*opt.pag_incore = INCORE_NO;*/
	    opt.pag_incore = (h->flags & HIS_MMAP) ? INCORE_MMAP : INCORE_NO;
	    opt.exists_incore = (h->flags & HIS_MMAP) ? INCORE_MMAP : INCORE_NO;

# if defined(MMAP_NEEDS_MSYNC) && INND_DBZINCORE == 1
	    /* Systems that have MMAP_NEEDS_MSYNC defined will have their
	       on-disk copies out of sync with the mmap'ed copies most of
	       the time.  So if innd is using INCORE_MMAP, then we force
	       everything else to use it, too (unless we're on NFS) */
	    if(!innconf->nfsreader) {
		opt.pag_incore = INCORE_MMAP;
		opt.exists_incore = INCORE_MMAP;
	    }
# endif
#endif
	}
	dbzsetoptions(opt);
	if (h->flags & HIS_CREAT) {
	    size_t npairs;
		
	    /* must only do this once! */
	    h->flags &= ~HIS_CREAT;
	    npairs = (h->npairs == -1) ? 0 : h->npairs;
	    if (!dbzfresh(h->histpath, dbzsize(npairs))) {
		hisv6_seterror(h, concat("can't dbzfresh ", h->histpath, " ",
					  strerror(errno), NULL));
		hisv6_closefiles(h);
		goto fail;
	    }
	} else if (!dbzinit(h->histpath)) {
	    hisv6_seterror(h, concat("can't dbzinit ", h->histpath, " ",
				      strerror(errno), NULL));
	    hisv6_closefiles(h);
	    goto fail;
	}
    }
    h->nextcheck = hisv6_nextcheck(h, TMRnow());
    r = true;
 fail:
    return r;
}


/*
** check if the history file has changed, if so rotate to the new
** history file. Returns false on failure (which is probably fatal as
** we'll have closed the files)
*/
static bool
hisv6_checkfiles(struct hisv6 *h)
{
    unsigned long t = TMRnow();

    if (h->statinterval == 0)
	return true;

    if (h->readfd == -1) {
	/* this can happen if a previous checkfiles() has failed to
	 * reopen the handles, but our caller hasn't realised... */
	hisv6_closefiles(h);
	if (!hisv6_reopen(h)) {
	    hisv6_closefiles(h);
	    return false;
	}
    }
    if (seq_lcompare(t, h->nextcheck) == 1) {
	struct stat st;

	if (stat(h->histpath, &st) == 0 &&
	    (st.st_ino != h->st.st_ino ||
	     st.st_dev != h->st.st_dev)) {
	    /* there's a possible race on the history file here... */
	    hisv6_closefiles(h);
	    if (!hisv6_reopen(h)) {
		hisv6_closefiles(h);
		return false;
	    }
	    h->st = st;
	}
	h->nextcheck = hisv6_nextcheck(h, t);
    }
    return true;
}


/*
**  dispose (and clean up) an existing history structure
*/
static bool
hisv6_dispose(struct hisv6 *h)
{
    bool r;

    r = hisv6_closefiles(h);
    if (h->histpath) {
	free(h->histpath);
	h->histpath = NULL;
    }

    free(h);
    return r;
}


/*
**  return a newly constructed, but empty, history structure
*/
static struct hisv6 *
hisv6_new(const char *path, int flags, struct history *history)
{
    struct hisv6 *h;

    h = xmalloc(sizeof *h);
    h->histpath = path ? xstrdup(path) : NULL;
    h->flags = flags;
    h->writefp = NULL;
    h->offset = 0;
    h->history = history;
    h->readfd = -1;
    h->nextcheck = 0;
    h->statinterval = 0;
    h->npairs = 0;
    h->dirty = 0;
    h->synccount = 0;
    h->st.st_ino = (ino_t)-1;
    h->st.st_dev = (dev_t)-1;
    return h;
}


/*
**  open the history database identified by path in mode flags
*/
void *
hisv6_open(const char *path, int flags, struct history *history)
{
    struct hisv6 *h;

    his_logger("HISsetup begin", S_HISsetup);

    h = hisv6_new(path, flags, history);
    if (path) {
	if (!hisv6_reopen(h)) {
	    hisv6_dispose(h);
	    h = NULL;
	}
    }
    his_logger("HISsetup end", S_HISsetup);
    return h;
}


/*
**  close and free a history handle
*/
bool
hisv6_close(void *history)
{
    struct hisv6 *h = history;
    bool r;

    his_logger("HISclose begin", S_HISclose);
    r = hisv6_dispose(h);
    his_logger("HISclose end", S_HISclose);
    return r;
}


/*
**  synchronise any outstanding history changes to disk
*/
bool
hisv6_sync(void *history)
{
    struct hisv6 *h = history;
    bool r = true;

    if (h->writefp != NULL) {
	his_logger("HISsync begin", S_HISsync);
	if (fflush(h->writefp) == EOF) {
	    hisv6_seterror(h, concat("error on history ",
				      h->histpath, " ",
				      strerror(errno), NULL));
	    r = false;
	}
	if (h->dirty && h == hisv6_dbzowner) {
	    if (!dbzsync()) {
		hisv6_seterror(h, concat("can't dbzsync ", h->histpath,
					  " ", strerror(errno), NULL));
		r = false;
	    } else {
		h->dirty = 0;
	    }
	}
	his_logger("HISsync end", S_HISsync);
    }
    return r;
}


/*
**  fetch the line associated with `hash' in the history database into
**  buf; buf must be at least HISV6_MAXLINE+1 bytes. `poff' is filled
**  with the offset of the line in the history file.
*/
static bool
hisv6_fetchline(struct hisv6 *h, const HASH *hash, char *buf, off_t *poff)
{
    off_t offset;
    bool r;

    if (h != hisv6_dbzowner) {
	hisv6_seterror(h, concat("dbz not open for this history file ",
				  h->histpath, NULL));
	return false;
    }
    if ((h->flags & (HIS_RDWR | HIS_INCORE)) == (HIS_RDWR | HIS_INCORE)) {
	/* need to fflush as we may be reading uncommitted data
	   written via writefp */
	if (fflush(h->writefp) == EOF) {
	    hisv6_seterror(h, concat("error on history ",
				      h->histpath, " ",
				      strerror(errno), NULL));
	    r = false;
	    goto fail;
	}
    }

    /* Get the seek value into the history file. */
    errno = 0;
    r = dbzfetch(*hash, &offset);
#ifdef ESTALE
    /* If your history is on NFS need to deal with stale NFS
     * handles */
    if (!r && errno == ESTALE) {
	hisv6_closefiles(h);
	if (!hisv6_reopen(h)) {
	    hisv6_closefiles(h);
	    r = false;
	    goto fail;
	}
    }
#endif
    if (r) {
	ssize_t n;

	do {
	    n = pread(h->readfd, buf, HISV6_MAXLINE, offset);
#ifdef ESTALE
	    if (n == -1 && errno == ESTALE) {
		hisv6_closefiles(h);
		if (!hisv6_reopen(h)) {
		    hisv6_closefiles(h);
		    r = false;
		    goto fail;
		}
	    }
#endif
	} while (n == -1 && errno == EINTR);
	if (n >= HISV6_MINLINE) {
	    char *p;

	    buf[n] = '\0';
	    p = strchr(buf, '\n');
	    if (!p) {
		char location[HISV6_MAX_LOCATION];

		hisv6_errloc(location, (size_t)-1, offset);
		hisv6_seterror(h,
				concat("can't locate end of line in history ",
				       h->histpath, location,
				       NULL));
		r = false;
	    } else {
		*p = '\0';
		*poff = offset;
		r = true;
	    }
	} else {
	    char location[HISV6_MAX_LOCATION];

	    hisv6_errloc(location, (size_t)-1, offset);
	    hisv6_seterror(h, concat("line too short in history ",
				      h->histpath, location,
				      NULL));
	    r = false;

	}
    } else {
	/* not found */
	r = false;
    }
 fail:
    return r;
}


/*
**  lookup up the entry `key' in the history database, returning
**  arrived, posted and expires (for those which aren't NULL
**  pointers), and any storage token associated with the entry.
**
**  If any of arrived, posted or expires aren't available, return zero
**  for that component.
*/
bool
hisv6_lookup(void *history, const char *key, time_t *arrived,
	     time_t *posted, time_t *expires, TOKEN *token)
{
    struct hisv6 *h = history;
    HASH messageid;
    bool r;
    off_t offset;
    char buf[HISV6_MAXLINE + 1];

    his_logger("HISfilesfor begin", S_HISfilesfor);
    hisv6_checkfiles(h);

    messageid = HashMessageID(key);
    r = hisv6_fetchline(h, &messageid, buf, &offset);
    if (r == true) {
	int status;
	const char *error;

	status = hisv6_splitline(buf, &error, NULL,
				  arrived, posted, expires, token);
	if (status < 0) {
	    char location[HISV6_MAX_LOCATION];

	    hisv6_errloc(location, (size_t)-1, offset);
	    hisv6_seterror(h, concat(error, " ",
				      h->histpath, location,
				      NULL));
	    r = false;
	} else {
	    /* if we have a token then we have the article */
	    r = !!(status & HISV6_HAVE_TOKEN);
	}
    }
    his_logger("HISfilesfor end", S_HISfilesfor);
    return r;
}


/*
**  check `key' has been seen in this history database
*/
bool
hisv6_check(void *history, const char *key)
{
    struct hisv6 *h = history;
    bool r;    
    HASH hash;
    
    if (h != hisv6_dbzowner) {
	hisv6_seterror(h, concat("dbz not open for this history file ",
				  h->histpath, NULL));
	return false;
    }

    his_logger("HIShavearticle begin", S_HIShavearticle);
    hisv6_checkfiles(h);
    hash = HashMessageID(key);
    r = dbzexists(hash);
    his_logger("HIShavearticle end", S_HIShavearticle);
    return r;
}


/*
**  Format a history line.  s should hold at least HISV6_MAXLINE + 1
**  characters (to allow for the nul).  Returns the length of the data
**  written, 0 if there was some error or if the data was too long to write.
*/
static int
hisv6_formatline(char *s, const HASH *hash, time_t arrived,
		  time_t posted, time_t expires, const TOKEN *token)
{
    int i;
    const char *hashtext = HashToText(*hash);

    if (token == NULL) {
	i = snprintf(s, HISV6_MAXLINE, "[%s]%c%lu%c%c\n",
		     hashtext, HISV6_FIELDSEP,
		     (unsigned long)arrived, HISV6_SUBFIELDSEP, HISV6_NOEXP);
    } else {
	const char *texttok;

	texttok = TokenToText(*token);
	if (expires <= 0) {
	    i = snprintf(s, HISV6_MAXLINE, "[%s]%c%lu%c%c%c%lu%c%s\n",
			 hashtext, HISV6_FIELDSEP,
			 (unsigned long)arrived, HISV6_SUBFIELDSEP,
			 HISV6_NOEXP, HISV6_SUBFIELDSEP,
			 (unsigned long)posted, HISV6_FIELDSEP,
			 texttok);
	} else {
	    i = snprintf(s, HISV6_MAXLINE, "[%s]%c%lu%c%lu%c%lu%c%s\n",
			 hashtext, HISV6_FIELDSEP,
			 (unsigned long)arrived, HISV6_SUBFIELDSEP,
			 (unsigned long)expires, HISV6_SUBFIELDSEP,
			 (unsigned long)posted, HISV6_FIELDSEP,
			 texttok);
	}
    }
    if (i < 0 || i >= HISV6_MAXLINE)
	return 0;
    return i;
}


/*
**  write the hash and offset to the dbz
*/
static bool
hisv6_writedbz(struct hisv6 *h, const HASH *hash, off_t offset)
{
    bool r;
    char location[HISV6_MAX_LOCATION];
    const char *error;

    /* store the offset in the database */
    switch (dbzstore(*hash, offset)) {
    case DBZSTORE_EXISTS:
	error = "dbzstore duplicate message-id ";
	/* not `false' so that we duplicate the pre-existing
	   behaviour */
	r = true;
	break;

    case DBZSTORE_ERROR:
	error = "dbzstore error ";
	r = false;
	break;

    default:
	error = NULL;
	r = true;
	break;
    }
    if (error) {
	hisv6_errloc(location, (size_t)-1, offset);
	hisv6_seterror(h, concat(error, h->histpath,
				  ":[", HashToText(*hash), "]",
				  location, " ", strerror(errno), NULL));
    }
    if (r && h->synccount != 0 && ++h->dirty >= h->synccount)
	r = hisv6_sync(h);

    return r;
}


/*
**  write a history entry, hash, with times arrived, posted and
**  expires, and storage token.
*/
static bool
hisv6_writeline(struct hisv6 *h, const HASH *hash, time_t arrived,
		time_t posted, time_t expires, const TOKEN *token)
{
    bool r;
    size_t i, length;
    char hisline[HISV6_MAXLINE + 1];
    char location[HISV6_MAX_LOCATION];

    if (h != hisv6_dbzowner) {
	hisv6_seterror(h, concat("dbz not open for this history file ",
				  h->histpath, NULL));
	return false;
    }

    if (!(h->flags & HIS_RDWR)) {
	hisv6_seterror(h, concat("history not open for writing ",
				  h->histpath, NULL));
	return false;
    }

    length = hisv6_formatline(hisline, hash, arrived, posted, expires, token);
    if (length == 0) {
	hisv6_seterror(h, concat("error formatting history line ",
				  h->histpath, NULL));
	return false;
    }	

    i = fwrite(hisline, 1, length, h->writefp);

    /* If the write failed, the history line is now an orphan.  Attempt to
       rewind the write pointer to our offset to avoid leaving behind a
       partial write and desyncing the offset from our file position. */
    if (i < length ||
        (!(h->flags & HIS_INCORE) && fflush(h->writefp) == EOF)) {
	hisv6_errloc(location, (size_t)-1, h->offset);
	hisv6_seterror(h, concat("can't write history ", h->histpath,
				  location, " ", strerror(errno), NULL));
        if (fseeko(h->writefp, h->offset, SEEK_SET) == -1)
            h->offset += i;
	r = false;
	goto fail;
    }

    r = hisv6_writedbz(h, hash, h->offset);
    h->offset += length;     /* increment regardless of error from writedbz */
 fail:
    return r;
}


/*
**  write a history entry, key, with times arrived, posted and
**  expires, and storage token.
*/
bool
hisv6_write(void *history, const char *key, time_t arrived,
	    time_t posted, time_t expires, const TOKEN *token)
{
    struct hisv6 *h = history;
    HASH hash;
    bool r;

    his_logger("HISwrite begin", S_HISwrite);
    hash = HashMessageID(key);
    r = hisv6_writeline(h, &hash, arrived, posted, expires, token);
    his_logger("HISwrite end", S_HISwrite);
    return r;
}


/*
**  remember a history entry, key, with arrival time arrived.
*/
bool
hisv6_remember(void *history, const char *key, time_t arrived)
{
    struct hisv6 *h = history;
    HASH hash;
    bool r;

    his_logger("HISwrite begin", S_HISwrite);
    hash = HashMessageID(key);
    r = hisv6_writeline(h, &hash, arrived, 0, 0, NULL);
    his_logger("HISwrite end", S_HISwrite);
    return r;
}


/*
**  replace an existing history entry, `key', with times arrived,
**  posted and expires, and (optionally) storage token `token'. The
**  new history line must fit in the space allocated for the old one -
**  if it had previously just been HISremember()ed you'll almost
**  certainly lose.
*/
bool
hisv6_replace(void *history, const char *key, time_t arrived,
	      time_t posted, time_t expires, const TOKEN *token)
{
    struct hisv6 *h = history;
    HASH hash;
    bool r;
    off_t offset;
    char old[HISV6_MAXLINE + 1];

    if (!(h->flags & HIS_RDWR)) {
	hisv6_seterror(h, concat("history not open for writing ",
				  h->histpath, NULL));
	return false;
    }

    hash = HashMessageID(key);
    r = hisv6_fetchline(h, &hash, old, &offset);
    if (r == true) {
	char new[HISV6_MAXLINE + 1];

	if (hisv6_formatline(new, &hash, arrived, posted, expires,
                             token) == 0) {
 	    hisv6_seterror(h, concat("error formatting history line ",
				      h->histpath, NULL));
	    r = false;
	} else {
	    size_t oldlen, newlen;

	    oldlen = strlen(old);
	    newlen = strlen(new);
	    if (new > old) {
		hisv6_seterror(h, concat("new history line too long ",
					  h->histpath, NULL));
		r = false;
	    } else {
		ssize_t n;

		/* space fill any excess in the tail of new */
		memset(new + oldlen, ' ', oldlen - newlen);

		do {
		    n = pwrite(fileno(h->writefp), new, oldlen, offset);
		} while (n == -1 && errno == EINTR);
		if (n != oldlen) {
		    char location[HISV6_MAX_LOCATION];

		    hisv6_errloc(location, (size_t)-1, offset);
		    hisv6_seterror(h, concat("can't write history ",
					      h->histpath, location, " ",
					      strerror(errno), NULL));
		    r = false;
		}
	    }
	}
    }
    return r;
}


/*
**  traverse a history database, passing the pieces through a
**  callback; note that we have more parameters in the callback than
**  the public interface, we add the internal history struct and the
**  message hash so we can use those if we need them. If the callback
**  returns false we abort the traversal.
**/
static bool
hisv6_traverse(struct hisv6 *h, struct hisv6_walkstate *cookie,
	       const char *reason,
	       bool (*callback)(struct hisv6 *, void *, const HASH *hash,
				time_t, time_t, time_t,
				const TOKEN *))
{
    bool r = false;
    QIOSTATE *qp;
    void *p;
    size_t line;
    char location[HISV6_MAX_LOCATION];

    if ((qp = QIOopen(h->histpath)) == NULL) {
	hisv6_seterror(h, concat("can't QIOopen history file ",
				  h->histpath, strerror(errno), NULL));
	return false;
    }

    line = 1;
    /* we come back to again after we hit EOF for the first time, when
       we pause the server & clean up any lines which sneak through in
       the interim */
 again:
    while ((p = QIOread(qp)) != NULL) {
	time_t arrived, posted, expires;
	int status;
	TOKEN token;
	HASH hash;
	const char *error;

	status = hisv6_splitline(p, &error, &hash,
				  &arrived, &posted, &expires, &token);
	if (status > 0) {
	    r = (*callback)(h, cookie, &hash, arrived, posted, expires,
			    (status & HISV6_HAVE_TOKEN) ? &token : NULL);
	    if (r == false)
		hisv6_seterror(h, concat("callback failed ",
					  h->histpath, NULL));
	} else {
	    hisv6_errloc(location, line, (off_t)-1);
	    hisv6_seterror(h, concat(error, " ", h->histpath, location,
				      NULL));
	    /* if we're not ignoring errors set the status */
	    if (!cookie->ignore)
		r = false;
	}
	if (r == false)
	    goto fail;
	++line;
    }

    if (p == NULL) {
	/* read or line-format error? */
	if (QIOerror(qp) || QIOtoolong(qp)) {
	    hisv6_errloc(location, line, (off_t)-1);
	    if (QIOtoolong(qp)) {
		hisv6_seterror(h, concat("line too long ",
					 h->histpath, location, NULL));
		/* if we're not ignoring errors set the status */
		if (!cookie->ignore)
		    r = false;
	    } else {
		hisv6_seterror(h, concat("can't read line ",
					 h->histpath, location, " ",
					 strerror(errno), NULL));
		r = false;
	    }
	    if (r == false)
		goto fail;
	}

	/* must have been EOF, pause the server & clean up any
	 * stragglers */
	if (reason && !cookie->paused) {
	    if (ICCpause(reason) != 0) {
		hisv6_seterror(h, concat("can't pause server ",
					  h->histpath, strerror(errno), NULL));
		r = false;
		goto fail;
	    }
	    cookie->paused = true;
	    goto again;
	}
    }
 fail:
    QIOclose(qp);
    return r;
}


/*
**  internal callback used during hisv6_traverse; we just pass on the
**  parameters the user callback expects
**/
static bool
hisv6_traversecb(struct hisv6 *h UNUSED, void *cookie, const HASH *hash UNUSED,
		 time_t arrived, time_t posted, time_t expires,
		 const TOKEN *token)
{
    struct hisv6_walkstate *hiscookie = cookie;

    return (*hiscookie->cb.walk)(hiscookie->cookie,
				 arrived, posted, expires,
				 token);
}


/*
**  history API interface to the database traversal routine
*/
bool
hisv6_walk(void *history, const char *reason, void *cookie,
	   bool (*callback)(void *, time_t, time_t, time_t,
			    const TOKEN *))
{
    struct hisv6 *h = history;
    struct hisv6_walkstate hiscookie;
    bool r;

    /* our internal walk routine passes too many parameters, so add a
       wrapper */
    hiscookie.cb.walk = callback;
    hiscookie.cookie = cookie;
    hiscookie.new = NULL;
    hiscookie.paused = false;
    hiscookie.ignore = false;

    r = hisv6_traverse(h, &hiscookie, reason, hisv6_traversecb);

    return r;
}


/*
**  internal callback used during expire
**/
static bool
hisv6_expirecb(struct hisv6 *h, void *cookie, const HASH *hash,
		time_t arrived, time_t posted, time_t expires,
		const TOKEN *token)
{
    struct hisv6_walkstate *hiscookie = cookie;
    bool r = true;

    /* check if we've seen this message id already */
    if (hiscookie->new && dbzexists(*hash)) {
	/* continue after duplicates, it serious, but not fatal */
	hisv6_seterror(h, concat("duplicate message-id [",
				 HashToText(*hash), "] in history ",
				 hiscookie->new->histpath, NULL));
    } else {
	struct hisv6_walkstate *hiscookie = cookie;
	TOKEN ltoken, *t;

	/* if we have a token pass it to the discrimination function */
	if (token) {
	    bool keep;

	    /* make a local copy of the token so the callback can
	     * modify it */
	    ltoken = *token;
	    t = &ltoken;
	    keep = (*hiscookie->cb.expire)(hiscookie->cookie,
					   arrived, posted, expires,
					   t);
	    /* if the callback returns true, we should keep the
	       token for the time being, else we just remember
	       it */
	    if (keep == false) {
		t = NULL;
		posted = expires = 0;
	    }
	} else {
	    t = NULL;
	}
	if (hiscookie->new &&
	    (t != NULL || arrived >= hiscookie->threshold)) {
	    r = hisv6_writeline(hiscookie->new, hash,
				 arrived, posted, expires, t);
	}
    }
    return r;
}


/*
**  unlink files associated with the history structure h
*/
static bool
hisv6_unlink(struct hisv6 *h)
{
    bool r = true;
    char *p;

#ifdef DO_TAGGED_HASH
    p = concat(h->histpath, ".pag", NULL);
    r = (unlink(p) == 0) && r;
    free(p);
#else
    p = concat(h->histpath, ".index", NULL);
    r = (unlink(p) == 0) && r;
    free(p);

    p = concat(h->histpath, ".hash", NULL);
    r = (unlink(p) == 0) && r;
    free(p);
#endif
    
    p = concat(h->histpath, ".dir", NULL);
    r = (unlink(p) == 0) && r;
    free(p);

    r = (unlink(h->histpath) == 0) && r;
    return r;
}


/*
**  rename files associated with hold to hnew
*/
static bool
hisv6_rename(struct hisv6 *hold, struct hisv6 *hnew)
{
    bool r = true;
    char *old, *new;

#ifdef DO_TAGGED_HASH
    old = concat(hold->histpath, ".pag", NULL);
    new = concat(hnew->histpath, ".pag", NULL);
    r = (rename(old, new) == 0) && r;
    free(old);
    free(new);
#else
    old = concat(hold->histpath, ".index", NULL);
    new = concat(hnew->histpath, ".index", NULL);
    r = (rename(old, new) == 0) && r;
    free(old);
    free(new);

    old = concat(hold->histpath, ".hash", NULL);
    new = concat(hnew->histpath, ".hash", NULL);
    r = (rename(old, new) == 0) && r;
    free(old);
    free(new);
#endif
    
    old = concat(hold->histpath, ".dir", NULL);
    new = concat(hnew->histpath, ".dir", NULL);
    r = (rename(old, new) == 0) && r;
    free(old);
    free(new);

    r = (rename(hold->histpath, hnew->histpath) == 0) && r;
    return r;
}


/*
**  expire the history database, history.
*/
bool
hisv6_expire(void *history, const char *path, const char *reason,
	     bool writing, void *cookie, time_t threshold,
	     bool (*exists)(void *, time_t, time_t, time_t, TOKEN *))
{
    struct hisv6 *h = history, *hnew = NULL;
    const char *nhistory = NULL;
    dbzoptions opt;
    bool r;
    struct hisv6_walkstate hiscookie;

    /* this flag is always tested in the fail clause, so initialise it
       now */
    hiscookie.paused = false;

    /* during expire we ignore errors whilst reading the history file
     * so any errors in it get fixed automagically */
    hiscookie.ignore = true;

    if (writing && (h->flags & HIS_RDWR)) {
	hisv6_seterror(h, concat("can't expire from read/write history ",
				 h->histpath, NULL));
	r = false;
	goto fail;
    }

    if (writing) {
	/* form base name for new history file */
	if (path != NULL) {
	    nhistory = concat(path, ".n", NULL);
	} else {
	    nhistory = concat(h->histpath, ".n", NULL);
	}

	hnew = hisv6_new(nhistory, HIS_RDWR | HIS_INCORE, h->history);
	if (!hisv6_reopen(hnew)) {
	    hisv6_dispose(hnew);
	    hnew = NULL;
	    r = false;
	    goto fail;
	}

	/* this is icky... we can only have one dbz open at a time; we
	   really want to make dbz take a state structure. For now we'll
	   just close the existing one and create our new one they way we
	   need it */
	if (!hisv6_dbzclose(h)) {
	    r = false;
	    goto fail;
	}

	dbzgetoptions(&opt);
	opt.writethrough = false;
	opt.pag_incore = INCORE_MEM;
#ifndef	DO_TAGGED_HASH
	opt.exists_incore = INCORE_MEM;
#endif
	dbzsetoptions(opt);

	if (h->npairs == 0) {
	    if (!dbzagain(hnew->histpath, h->histpath)) {
		hisv6_seterror(h, concat("can't dbzagain ",
					 hnew->histpath, ":", h->histpath, 
					 strerror(errno), NULL));
		r = false;
		goto fail;
	    }
	} else {
	    size_t npairs;

	    npairs = (h->npairs == -1) ? 0 : h->npairs;
	    if (!dbzfresh(hnew->histpath, dbzsize(npairs))) {
		hisv6_seterror(h, concat("can't dbzfresh ",
					 hnew->histpath, ":", h->histpath, 
					 strerror(errno), NULL));
		r = false;
		goto fail;
	    }
	}
	hisv6_dbzowner = hnew;
    }

    /* set up the callback handler */
    hiscookie.cb.expire = exists;
    hiscookie.cookie = cookie;
    hiscookie.new = hnew;
    hiscookie.threshold = threshold;
    r = hisv6_traverse(h, &hiscookie, reason, hisv6_expirecb);

 fail:
    if (writing) {
	if (hnew && !hisv6_closefiles(hnew)) {
	    /* error will already have been set */
	    r = false;
	}

	/* reopen will synchronise the dbz stuff for us */
	if (!hisv6_closefiles(h)) {
	    /* error will already have been set */
	    r = false;
	}

	if (r) {
	    /* if the new path was explicitly specified don't move the
	       files around, our caller is planning to do it out of
	       band */
	    if (path == NULL) {
		/* unlink the old files */
		r = hisv6_unlink(h);
	    
		if (r) {
		    r = hisv6_rename(hnew, h);
		}
	    }
	} else if (hnew) {
	    /* something went pear shaped, unlink the new files */
	    hisv6_unlink(hnew);
	}

	/* re-enable dbz on the old history file */
	if (!hisv6_reopen(h)) {
	    hisv6_closefiles(h);
	}
    }

    if (hnew && !hisv6_dispose(hnew))
	r = false;
    if (nhistory && nhistory != path)
	free((char *)nhistory);
    if (r == false && hiscookie.paused)
	ICCgo(reason);
    return r;
}


/*
**  control interface
*/
bool
hisv6_ctl(void *history, int selector, void *val)
{
    struct hisv6 *h = history;
    bool r = true;

    switch (selector) {
    case HISCTLG_PATH:
	*(char **)val = h->histpath;
	break;

    case HISCTLS_PATH:
	if (h->histpath) {
	    hisv6_seterror(h, concat("path already set in handle", NULL));
	    r = false;
	} else {
	    h->histpath = xstrdup((char *)val);
	    if (!hisv6_reopen(h)) {
		free(h->histpath);
		h->histpath = NULL;
		r = false;
	    }
	}
	break;

    case HISCTLS_STATINTERVAL:
	h->statinterval = *(time_t *)val * 1000;
	break;

    case HISCTLS_SYNCCOUNT:
	h->synccount = *(size_t *)val;
	break;

    case HISCTLS_NPAIRS:
	h->npairs = (ssize_t)*(size_t *)val;
	break;

    case HISCTLS_IGNOREOLD:
	if (h->npairs == 0 && *(bool *)val) {
	    h->npairs = -1;
	} else if (h->npairs == -1 && !*(bool *)val) {
	    h->npairs = 0;
	}
	break;

    default:
	/* deliberately doesn't call hisv6_seterror as we don't want
	 * to spam the error log if someone's passing in stuff which
	 * would be relevant to a different history manager */
	r = false;
	break;
    }
    return r;
}
