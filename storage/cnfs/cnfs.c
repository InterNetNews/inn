/*  $Id$
**
**  cyclic news file system
*/

#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <dirent.h>
#include <ctype.h>
#include <clibrary.h>
#include <errno.h>
#include <limits.h>
#include <logging.h>
#include <macros.h>
#include <configdata.h>
#include <libinn.h>
#include <methods.h>

#include <configdata.h>
#include <interface.h>
#include "cnfs.h"
#include "cnfs-private.h"
#include "paths.h"

typedef struct {
    /**** Stuff to be cleaned up when we're done with the article */
    char		*base;		/* Base of mmap()ed art */
    int			len;		/* Length of article (and thus
					   mmap()ed art */
    CYCBUFF		*cycbuff;	/* pointer to current CYCBUFF */
    CYCBUFF_OFF_T	offset;		/* offset to current article */
    BOOL		rollover;	/* true if the search is rollovered */
} PRIV_CNFS;

STATIC char LocalLogName[] = "CNFS-sm";
STATIC CYCBUFF		*cycbufftab = (CYCBUFF *)NULL;
STATIC METACYCBUFF 	*metacycbufftab = (METACYCBUFF *)NULL;
STATIC CNFSEXPIRERULES	*metaexprulestab = (CNFSEXPIRERULES *)NULL;
STATIC long		pagesize = 0;

STATIC TOKEN CNFSMakeToken(char *cycbuffname, CYCBUFF_OFF_T offset,
		       U_INT32_T cycnum, STORAGECLASS class) {
    TOKEN               token;
    INT32_T		int32;

    memset(&token, '\0', sizeof(token));
    /*
    ** XXX We'll assume that TOKENSIZE is 16 bytes and that we divvy it
    ** up as: 8 bytes for cycbuffname, 4 bytes for offset, 4 bytes
    ** for cycnum.  See also: CNFSBreakToken() for hard-coded constants.
    */
    token.type = TOKEN_CNFS;
    token.class = class;
    memcpy(token.token, cycbuffname, CNFSMAXCYCBUFFNAME);
    int32 = htonl(offset / CNFS_BLOCKSIZE);
    memcpy(&token.token[8], &int32, sizeof(int32));
    int32 = htonl(cycnum);
    memcpy(&token.token[12], &int32, sizeof(int32));
    return token;
}

/*
** NOTE: We assume that cycbuffname is 9 bytes long.
*/

STATIC BOOL CNFSBreakToken(TOKEN token, char *cycbuffname,
			   CYCBUFF_OFF_T *offset, U_INT32_T *cycnum) {
    INT32_T	int32;

    if (cycbuffname == NULL || offset == NULL || cycnum == NULL) {
	syslog(L_ERROR, "%s: BreakToken: invalid argument",
	       LocalLogName, cycbuffname);
	SMseterror(SMERR_INTERNAL, "BreakToken: invalid argument");
	return FALSE;
    }
    memcpy(cycbuffname, token.token, CNFSMAXCYCBUFFNAME);
    *(cycbuffname + CNFSMAXCYCBUFFNAME) = '\0';	/* Just to be paranoid */
    memcpy(&int32, &token.token[8], sizeof(int32));
    *offset = ntohl(int32) * CNFS_BLOCKSIZE;
    memcpy(&int32, &token.token[12], sizeof(int32));
    *cycnum = ntohl(int32);
    return TRUE;
}

STATIC char hextbl[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
			'a', 'b', 'c', 'd', 'e', 'f'};

/*
** CNFSofft2hex -- Given an argument of type CYCBUFF_OFF_T, return
**	a static ASCII string representing its value in hexadecimal.
**
**	If "leadingzeros" is true, the number returned will have leading 0's.
*/

STATIC char * CNFSofft2hex(CYCBUFF_OFF_T offset, BOOL leadingzeros) {
    static char	buf[24];
    char	*p;

    if (sizeof(CYCBUFF_OFF_T) <= 4) {
	sprintf(buf, (leadingzeros) ? "%016lx" : "%lx", offset);
    } else { 
	int	i;

	for (i = 0; i < CNFSLASIZ; i++)
	    buf[i] = '0';	/* Pad with zeros to start */
	for (i = CNFSLASIZ - 1; i >= 0; i--) {
	    buf[i] = hextbl[offset & 0xf];
	    offset >>= 4;
	}
    }
    if (! leadingzeros) {
	for (p = buf; *p == '0'; p++)
	    ;
	if (*p != '\0')
		return p;
	else
		return p - 1;	/* We converted a "0" and then bypassed all
				   the zeros */
    } else 
	return buf;
}

/*
** CNFShex2offt -- Given an ASCII string containing a hexadecimal representation
**	of a CYCBUFF_OFF_T, return a CYCBUFF_OFF_T.
*/

STATIC CYCBUFF_OFF_T CNFShex2offt(char *hex) {
    if (sizeof(CYCBUFF_OFF_T) <= 4) {
	CYCBUFF_OFF_T	rpofft;
	/* I'm lazy */
	sscanf(hex, "%lx", &rpofft);
	return rpofft;
    } else {
	char		diff;
	CYCBUFF_OFF_T	n = (CYCBUFF_OFF_T) 0;

	for (; *hex != '\0'; hex++) {
	    if (*hex >= '0' && *hex <= '9')
		diff = '0';
	    else if (*hex >= 'a' && *hex <= 'f')
		diff = 'a' - 10;
	    else if (*hex >= 'A' && *hex <= 'F')
		diff = 'A' - 10;
	    else {
		/*
		** We used to have a syslog() message here, but the case
		** where we land here because of a ":" happens, er, often.
		*/
		break;
	    }
	    n += (*hex - diff);
	    if (isalnum(*(hex + 1)))
		n <<= 4;
	}
	return n;
    }
}

STATIC void CNFScleancycbuff(void) {
    CYCBUFF	*cycbuff, *nextcycbuff;

    for (cycbuff = cycbufftab; cycbuff != (CYCBUFF *)NULL;) {
      nextcycbuff = cycbuff->next;
      DISPOSE(cycbuff);
      cycbuff = nextcycbuff;
    }
    cycbufftab = (CYCBUFF *)NULL;
}

STATIC void CNFScleanmetacycbuff(void) {
    METACYCBUFF	*metacycbuff, *nextmetacycbuff;

    for (metacycbuff = metacycbufftab; metacycbuff != (METACYCBUFF *)NULL;) {
      nextmetacycbuff = metacycbuff->next;
      DISPOSE(metacycbuff->members);
      DISPOSE(metacycbuff->name);
      DISPOSE(metacycbuff);
      metacycbuff = nextmetacycbuff;
    }
    metacycbufftab = (METACYCBUFF *)NULL;
}

STATIC void CNFScleanexpirerule(void) {
    CNFSEXPIRERULES	*metaexprule, *nextmetaexprule;

    for (metaexprule = metaexprulestab; metaexprule != (CNFSEXPIRERULES *)NULL;) {
      nextmetaexprule = metaexprule->next;
      DISPOSE(metaexprule);
      metaexprule = nextmetaexprule;
    }
    metaexprulestab = (CNFSEXPIRERULES *)NULL;
}

STATIC CYCBUFF *CNFSgetcycbuffbyname(char *name) {
    CYCBUFF	*cycbuff;
 
    if (name == NULL)
	return NULL;
    for (cycbuff = cycbufftab; cycbuff != (CYCBUFF *)NULL; cycbuff = cycbuff->next)
	if (strcmp(name, cycbuff->name) == 0) 
	    return cycbuff;
    return NULL;
}

STATIC METACYCBUFF *CNFSgetmetacycbuffbyname(char *name) {
  METACYCBUFF	*metacycbuff;

  if (name == NULL)
    return NULL;
  for (metacycbuff = metacycbufftab; metacycbuff != (METACYCBUFF *)NULL; metacycbuff = metacycbuff->next)
    if (strcmp(name, metacycbuff->name) == 0) 
      return metacycbuff;
  return NULL;
}

STATIC BOOL CNFSflushhead(CYCBUFF *cycbuff) {
  int			b;
  CYCBUFFEXTERN		rpx;

  if (!cycbuff->needflush)
    return TRUE;
  memset(&rpx, 0, sizeof(CYCBUFFEXTERN));
  if (CNFSseek(cycbuff->fdrdwr, (CYCBUFF_OFF_T) 0, SEEK_SET) < 0) {
    syslog(L_ERROR, "CNFSflushhead: magic CNFSseek failed: %m");
    return FALSE;
  }
  if (cycbuff->magicver == 3) {
    cycbuff->updated = time(NULL);
    strncpy(rpx.magic, CNFS_MAGICV3, strlen(CNFS_MAGICV3));
    strncpy(rpx.name, cycbuff->name, CNFSNASIZ);
    strncpy(rpx.path, cycbuff->path, CNFSPASIZ);
    /* Don't use sprintf() directly ... the terminating '\0' causes grief */
    strncpy(rpx.lena, CNFSofft2hex(cycbuff->len, TRUE), CNFSLASIZ);
    strncpy(rpx.freea, CNFSofft2hex(cycbuff->free, TRUE), CNFSLASIZ);
    strncpy(rpx.cyclenuma, CNFSofft2hex(cycbuff->cyclenum, TRUE), CNFSLASIZ);
    strncpy(rpx.updateda, CNFSofft2hex(cycbuff->updated, TRUE), CNFSLASIZ);
    if ((b = write(cycbuff->fdrdwr, &rpx, sizeof(CYCBUFFEXTERN))) != sizeof(CYCBUFFEXTERN)) {
      syslog(L_ERROR, "%s: CNFSflushhead: write failed (%d bytes): %m", LocalLogName, b);
      return FALSE;
    }
    cycbuff->needflush = FALSE;
  } else {
    syslog(L_ERROR, "%s: CNFSflushhead: bogus magicver for %s: %d",
      LocalLogName, cycbuff->name, cycbuff->magicver);
    return FALSE;
  }
  return TRUE;
}

STATIC void CNFSflushallheads(void) {
  CYCBUFF	*cycbuff;

  for (cycbuff = cycbufftab; cycbuff != (CYCBUFF *)NULL; cycbuff = cycbuff->next) {
    if (cycbuff->needflush)
	syslog(L_NOTICE, "%s: CNFSflushallheads: flushing %s", LocalLogName, cycbuff->name);
    (void)CNFSflushhead(cycbuff);
  }
}

/*
** CNFSReadFreeAndCycle() -- Read from disk the current values of CYCBUFF's
**	free pointer and cycle number.  Return 1 on success, 0 otherwise.
*/

STATIC void CNFSReadFreeAndCycle(CYCBUFF *cycbuff) {
    CYCBUFFEXTERN	rpx;
    char		buf[64];

    if (CNFSseek(cycbuff->fdrd, (CYCBUFF_OFF_T) 0, SEEK_SET) < 0) {
	syslog(L_ERROR, "CNFSReadFreeAndCycle: magic lseek failed: %m");
	SMseterror(SMERR_UNDEFINED, NULL);
	return;
    }
    if (read(cycbuff->fdrd, &rpx, sizeof(CYCBUFFEXTERN)) != sizeof(rpx)) {
	syslog(L_ERROR, "CNFSReadFreeAndCycle: magic read failed: %m");
	SMseterror(SMERR_UNDEFINED, NULL);
	return;
    }
    /* Sanity checks are not needed since CNFSinit_disks() has already done. */
    buf[CNFSLASIZ] = '\0';
    strncpy(buf, rpx.freea, CNFSLASIZ);
    cycbuff->free = CNFShex2offt(buf);
    buf[CNFSLASIZ] = '\0';
    strncpy(buf, rpx.updateda, CNFSLASIZ);
    cycbuff->updated = CNFShex2offt(buf);
    buf[CNFSLASIZ] = '\0';
    strncpy(buf, rpx.cyclenuma, CNFSLASIZ);
    cycbuff->cyclenum = CNFShex2offt(buf);
    return;
}

STATIC BOOL CNFSparse_part_line(char *l) {
  char		*p;
  struct stat	sb;
  CYCBUFF_OFF_T	len, minartoffset;
  int		tonextblock;
  CYCBUFF	*cycbuff, *tmp;

  /* Symbolic cnfs partition name */
  if ((p = strchr(l, ':')) == NULL || (p - l <= 0 || p - l > CNFSMAXCYCBUFFNAME)) {
    syslog(L_ERROR, "%s: bad cycbuff name in line '%s'", LocalLogName, l);
    return FALSE;
  }
  *p = '\0';
  cycbuff = NEW(CYCBUFF, 1);
  memset(cycbuff->name, '\0', CNFSNASIZ);
  strcpy(cycbuff->name, l);
  l = ++p;

  /* Path to cnfs partition */
  if ((p = strchr(l, ':')) == NULL || p - l <= 0) {
    syslog(L_ERROR, "%s: bad pathname in line '%s'", LocalLogName, l);
    DISPOSE(cycbuff);
    return FALSE;
  }
  *p = '\0';
  memset(cycbuff->path, '\0', CNFSPASIZ);
  strcpy(cycbuff->path, l);
  if (stat(cycbuff->path, &sb) < 0) {
    syslog(L_ERROR, "%s: file '%s' does not exist, ignoring '%s' cycbuff",
	   LocalLogName, cycbuff->path, cycbuff->name);
    DISPOSE(cycbuff);
    return FALSE;
  }
  l = ++p;

  /* Length/size of symbolic partition */
  len = atoi(l) * 1024;		/* This value in KB in decimal */
  if (len != sb.st_size) {
    syslog(L_ERROR, "%s: bad length '%ld' for '%s' cycbuff(%ld)",
	   /* Danger... */ LocalLogName, len, cycbuff->name);
    DISPOSE(cycbuff);
    return FALSE;
  }
  cycbuff->len = len;
  cycbuff->fdrd = -1;
  cycbuff->fdrdwr = -1;
  cycbuff->next = (CYCBUFF *)NULL;
  cycbuff->needflush = FALSE;
  /*
  ** The minimum article offset will be the size of the bitfield itself,
  ** len / (blocksize * 8), plus however many additional blocks the CYCBUFF
  ** external header occupies ... then round up to the next block.
  */
  minartoffset =
      cycbuff->len / (CNFS_BLOCKSIZE * 8) + CNFS_BEFOREBITF;
  tonextblock = CNFS_BLOCKSIZE - (minartoffset & (CNFS_BLOCKSIZE - 1));
  cycbuff->minartoffset = minartoffset + tonextblock;

  if (cycbufftab == (CYCBUFF *)NULL)
    cycbufftab = cycbuff;
  else {
    for (tmp = cycbufftab; tmp->next != (CYCBUFF *)NULL; tmp = tmp->next);
    tmp->next = cycbuff;
  }
  /* Done! */
  return TRUE;
}

STATIC BOOL CNFSparse_metapart_line(char *l) {
  char		*p, *cycbuff;
  int		rpi;
  CYCBUFF	*rp;
  METACYCBUFF	*metacycbuff, *tmp;

  /* Symbolic metacycbuff name */
  if ((p = strchr(l, ':')) == NULL || p - l <= 0) {
    syslog(L_ERROR, "%s: bad partition name in line '%s'", LocalLogName, l);
    return FALSE;
  }
  *p = '\0';
  metacycbuff = NEW(METACYCBUFF, 1);
  metacycbuff->members = (CYCBUFF **)NULL;
  metacycbuff->count = 0;
  metacycbuff->name = COPY(l);
  metacycbuff->next = (METACYCBUFF *)NULL;
  l = ++p;

  /* Cycbuff list */
  while ((p = strchr(l, ',')) != NULL && p - l > 0) {
    *p = '\0';
    cycbuff = l;
    l = ++p;
    if ((rp = CNFSgetcycbuffbyname(cycbuff)) == NULL) {
      syslog(L_ERROR, "%s: bogus cycbuff '%s' (metacycbuff '%s')",
	     LocalLogName, cycbuff, metacycbuff->name);
      DISPOSE(metacycbuff->members);
      DISPOSE(metacycbuff->name);
      DISPOSE(metacycbuff);
      return FALSE;
    }
    if (metacycbuff->count == 0)
      metacycbuff->members = NEW(CYCBUFF *, 1);
    else 
      RENEW(metacycbuff->members, CYCBUFF *, metacycbuff->count + 1);
    metacycbuff->members[metacycbuff->count++] = rp;
  }
  /* Gotta deal with the last cycbuff on the list */
  cycbuff = l;
  if ((rp = CNFSgetcycbuffbyname(cycbuff)) == NULL) {
    syslog(L_ERROR, "%s: bogus cycbuff '%s' (metacycbuff '%s')",
	   LocalLogName, cycbuff, metacycbuff->name);
    DISPOSE(metacycbuff->members);
    DISPOSE(metacycbuff->name);
    DISPOSE(metacycbuff);
    return FALSE;
  } else {
    if (metacycbuff->count == 0)
      metacycbuff->members = NEW(CYCBUFF *, 1);
    else 
      RENEW(metacycbuff->members, CYCBUFF *, metacycbuff->count + 1);
    metacycbuff->members[metacycbuff->count++] = rp;
  }
  
  if (metacycbuff->count == 0) {
    syslog(L_ERROR, "%s: no cycbuffs assigned to cycbuff '%s'",
	   LocalLogName, metacycbuff->name);
    DISPOSE(metacycbuff->name);
    DISPOSE(metacycbuff);
    return FALSE;
  }
  if (metacycbufftab == (METACYCBUFF *)NULL)
    metacycbufftab = metacycbuff;
  else {
    for (tmp = metacycbufftab; tmp->next != (METACYCBUFF *)NULL; tmp = tmp->next);
    tmp->next = metacycbuff;
  }
  /* DONE! */
  return TRUE;
}

STATIC BOOL CNFSparse_groups_line() {
  METACYCBUFF	*mrp;
  STORAGE_SUB	*sub = (STORAGE_SUB *)NULL;
  CNFSEXPIRERULES	*metaexprule, *tmp;

  sub = SMGetConfig(TOKEN_CNFS, sub);
  for (;sub != (STORAGE_SUB *)NULL; sub = SMGetConfig(TOKEN_CNFS, sub)) {
    if (sub->options == (char *)NULL) {
      syslog(L_ERROR, "%s: storage.ctl additional field is missing",
	   LocalLogName);
      CNFScleanexpirerule();
      return FALSE;
    }
    if ((mrp = CNFSgetmetacycbuffbyname(sub->options)) == NULL) {
      syslog(L_ERROR, "%s: storage.ctl additional field '%s' undefined",
	   LocalLogName, sub->options);
      CNFScleanexpirerule();
      return FALSE;
    }
    metaexprule = NEW(CNFSEXPIRERULES, 1);
    metaexprule->class = sub->class;
    metaexprule->dest = mrp;
    metaexprule->next = (CNFSEXPIRERULES *)NULL;
    if (metaexprulestab == (CNFSEXPIRERULES *)NULL)
      metaexprulestab = metaexprule;
    else {
      for (tmp = metaexprulestab; tmp->next != (CNFSEXPIRERULES *)NULL; tmp = tmp->next);
      tmp->next = metaexprule;
    }
  }
  /* DONE! */
  return TRUE;
}

/*
** CNFSinit_disks -- Finish initializing cycbufftab and metacycbufftab.
**	Called by "innd" only -- we open (and keep) a read/write
**	file descriptor for each CYCBUFF.
**
** Calling this function repeatedly shouldn't cause any harm
** speed-wise or bug-wise, as long as the caller is accessing the
** CYCBUFFs _read-only_.  If innd calls this function repeatedly,
** bad things will happen.
*/

STATIC BOOL CNFSinit_disks(void) {
  char		buf[64];
  CYCBUFFEXTERN	rpx;
  int		i, fd, bytes;
  CYCBUFF_OFF_T	tmpo;
  METACYCBUFF	*metacycbuff;
  CYCBUFF	*cycbuff;

  /*
  ** Discover the state of our cycbuffs.  If any of them are in icky shape,
  ** duck shamelessly & return FALSE.
  */

  for (cycbuff = cycbufftab; cycbuff != (CYCBUFF *)NULL; cycbuff = cycbuff->next) {
    if (strcmp(cycbuff->path, "/dev/null") == 0) {
	syslog(L_ERROR, "%s: ERROR opening '%s' is not available",
		LocalLogName, cycbuff->path);
	return FALSE;
    }
    if (cycbuff->fdrd < 0) {
	if ((fd = open(cycbuff->path, O_RDONLY)) < 0) {
	    syslog(L_ERROR, "%s: ERROR opening '%s' O_RDONLY : %m",
		   LocalLogName, cycbuff->path);
	    return FALSE;
	} else {
	    cycbuff->fdrd = fd;
	}
    }
    if (cycbuff->fdrdwr < 0) {
	if ((fd = open(cycbuff->path, O_RDWR)) < 0) {
	    syslog(L_ERROR, "%s: ERROR opening '%s' O_RDWR : %m",
		   LocalLogName, cycbuff->path);
	    return FALSE;
	} else {
	    cycbuff->fdrdwr = fd;
	}
    }
    if ((tmpo = CNFSseek(cycbuff->fdrd, (CYCBUFF_OFF_T) 0, SEEK_SET)) < 0) {
	syslog(L_ERROR, "%s: pre-magic read lseek failed: %m", LocalLogName);
	return FALSE;
    }
    if ((bytes = read(cycbuff->fdrd, &rpx, sizeof(CYCBUFFEXTERN))) != sizeof(rpx)) {
	syslog(L_ERROR, "%s: read magic failed %d bytes: %m", LocalLogName, bytes);
	return FALSE;
    }
    if (CNFSseek(cycbuff->fdrd, tmpo, SEEK_SET) != tmpo) {
	syslog(L_ERROR, "%s: post-magic read lseek to 0x%s failed: %m",
	       LocalLogName, CNFSofft2hex(tmpo, FALSE));
	return FALSE;
    }

    /*
    ** Much of this checking from previous revisions is (probably) bogus
    ** & buggy & particularly icky & unupdated.  Use at your own risk.  :-)
    */

    if (strncmp(rpx.magic, CNFS_MAGICV3, strlen(CNFS_MAGICV3)) == 0) {
	cycbuff->magicver = 3;
	if (strncmp(rpx.name, cycbuff->name, CNFSNASIZ) != 0) {
	    syslog(L_ERROR, "%s: Mismatch 3: read %s for cycbuff %s", LocalLogName,
		   rpx.name, cycbuff->name);
	    return FALSE;
	}
	if (strncmp(rpx.path, cycbuff->path, CNFSPASIZ) != 0) {
	    syslog(L_ERROR, "%s: Path mismatch: read %s for cycbuff %s",
		   LocalLogName, rpx.path, cycbuff->path);
	    return FALSE;
	}
	strncpy(buf, rpx.lena, CNFSLASIZ);
	buf[CNFSLASIZ] = '\0';
	tmpo = CNFShex2offt(buf);
	if (tmpo != cycbuff->len) {
	    syslog(L_ERROR, "%s: Mismatch: read 0x%s length for cycbuff %s",
		   LocalLogName, CNFSofft2hex(tmpo, FALSE), cycbuff->path);
	    return FALSE;
	}
	buf[CNFSLASIZ] = '\0';
	strncpy(buf, rpx.freea, CNFSLASIZ);
	cycbuff->free = CNFShex2offt(buf);
	buf[CNFSLASIZ] = '\0';
	strncpy(buf, rpx.updateda, CNFSLASIZ);
	cycbuff->updated = CNFShex2offt(buf);
	buf[CNFSLASIZ] = '\0';
	strncpy(buf, rpx.cyclenuma, CNFSLASIZ);
	cycbuff->cyclenum = CNFShex2offt(buf);
    } else {
	syslog(L_NOTICE,
		"%s: No magic cookie found for cycbuff %s, initializing",
		LocalLogName, cycbuff->name);
	cycbuff->magicver = 3;
	cycbuff->free = cycbuff->minartoffset;
	cycbuff->updated = 0;
	cycbuff->cyclenum = 1;
	cycbuff->needflush = TRUE;
	if (!CNFSflushhead(cycbuff))
	    return FALSE;
    }
    errno = 0;
    fd = cycbuff->fdrdwr;
    if ((cycbuff->bitfield =
	 mmap((caddr_t) 0, cycbuff->minartoffset, PROT_READ | PROT_WRITE,
	      MAP_SHARED, fd, (off_t) 0)) == (MMAP_PTR) -1 || errno != 0) {
	syslog(L_ERROR,
	       "%s: CNFSinitdisks: mmap for %s offset %d len %d failed: %m",
	       LocalLogName, cycbuff->path, 0, cycbuff->minartoffset);
	return FALSE;
    }
  }

  /*
  ** OK.  Time to figure out the state of our metacycbuffs...
  **
  */
  for (metacycbuff = metacycbufftab; metacycbuff != (METACYCBUFF *)NULL; metacycbuff = metacycbuff->next) {
    metacycbuff->memb_next = 0;
    metacycbuff->write_count = 0;		/* Let's not forget this */
  }
  return TRUE;
}

/*
** CNFSread_config() -- Read the cnfs partition/file configuration file.
**
** Oh, for the want of Perl!  My parser probably shows that I don't use
** C all that often anymore....
*/

STATIC BOOL CNFSread_config(void) {
    char	*config, *from, *to, **ctab = (char **)NULL;
    int		ctab_free = 0;	/* Index to next free slot in ctab */
    int		ctab_i;
    BOOL	metacycbufffound = FALSE;

    if ((config = ReadInFile(_PATH_CYCBUFFCONFIG, (struct stat *)NULL)) == NULL) {
	syslog(L_ERROR, "%s: cannot read %s", LocalLogName, _PATH_CYCBUFFCONFIG, NULL);
	DISPOSE(config);
	return FALSE;
    }
    for (from = to = config; *from; ) {
	if (ctab_free == 0)
	  ctab = NEW(char *, 1);
	else
	  RENEW(ctab, char *, ctab_free+1);
	if (*from == '#') {	/* Comment line? */
	    while (*from && *from != '\n')
		from++;				/* Skip past it */
	    from++;
	    continue;				/* Back to top of loop */
	}
	if (*from == '\n') {	/* End or just a blank line? */
	    from++;
	    continue;				/* Back to top of loop */
	}
	/* If we're here, we've got the beginning of a real entry */
	ctab[ctab_free++] = to = from;
	while (1) {
	    if (*from && *from == '\\' && *(from + 1) == '\n') {
		from += 2;		/* Skip past backslash+newline */
		while (*from && isspace(*from))
		    from++;
		continue;
	    }
	    if (*from && *from != '\n')
		*to++ = *from++;
	    if (*from == '\n') {
		*to++ = '\0';
		from++;
		break;
	    }
	    if (! *from)
		break;
	}
    }

    for (ctab_i = 0; ctab_i < ctab_free; ctab_i++) {
	if (strncmp(ctab[ctab_i], "cycbuff:", 8) == 0) {
	    if (metacycbufffound) {
		syslog(L_ERROR, "%s: all cycbuff entries shoud be before metacycbuff entries", LocalLogName);
		DISPOSE(config);
		DISPOSE(ctab);
		return FALSE;
	    }
	    if (!CNFSparse_part_line(ctab[ctab_i] + 8)) {
		DISPOSE(config);
		DISPOSE(ctab);
		return FALSE;
	    }
	} else if (strncmp(ctab[ctab_i], "metacycbuff:", 12) == 0) {
	    metacycbufffound = TRUE;
	    if (!CNFSparse_metapart_line(ctab[ctab_i] + 12)) {
		DISPOSE(config);
		DISPOSE(ctab);
		return FALSE;
	    }
	} else {
	    syslog(L_ERROR, "%s: Bogus metacycbuff config line '%s' ignored",
		   LocalLogName, ctab[ctab_i]);
	}
    }
    DISPOSE(config);
    DISPOSE(ctab);
    if (!CNFSparse_groups_line()) {
	return FALSE;
    }
    if (cycbufftab == (CYCBUFF *)NULL) {
	syslog(L_ERROR, "%s: zero cycbuffs defined", LocalLogName);
	return FALSE;
    }
    if (metacycbufftab == (METACYCBUFF *)NULL) {
	syslog(L_ERROR, "%s: zero metacycbuffs defined", LocalLogName);
	return FALSE;
    }
    return TRUE;
}

/*
**	Bit arithmetic by brute force.
**
**	XXXYYYXXX WARNING: the code below is not endian-neutral!
*/

typedef unsigned long	ULONG;

STATIC int CNFSUsedBlock(CYCBUFF *cycbuff, CYCBUFF_OFF_T offset,
	      BOOL set_operation, BOOL setbitvalue) {
    CYCBUFF_OFF_T	blocknum;
    CYCBUFF_OFF_T	longoffset;
    int			bitoffset;	/* From the 'left' side of the long */
    static int		uninitialized = 1;
    static int		longsize = sizeof(long);
    int	i;
    ULONG		bitlong, on, off, mask;
    static ULONG	onarray[64], offarray[64];

    if (uninitialized) {
	on = 1;
	off = on;
	off ^= ULONG_MAX;
	for (i = (longsize * 8) - 1; i >= 0; i--) {
	    onarray[i] = on;
	    offarray[i] = off;
	    on <<= 1;
	    off = on;
	    off ^= ULONG_MAX;
	}
	uninitialized = 0;
    }

    /* We allow bit-setting under minartoffset, but it better be FALSE */
    if ((offset < cycbuff->minartoffset && setbitvalue) ||
	offset > cycbuff->len) {
	char	bufoff[64], bufmin[64], bufmax[64];
	SMseterror(SMERR_INTERNAL, NULL);
	strcpy(bufoff, CNFSofft2hex(offset, FALSE));
	strcpy(bufmin, CNFSofft2hex(cycbuff->minartoffset, FALSE));
	strcpy(bufmax, CNFSofft2hex(cycbuff->len, FALSE));
	syslog(L_ERROR,
	       "%s: CNFSUsedBlock: invalid offset %s, min = %s, max = %s",
	       LocalLogName, bufoff, bufmin, bufmax);
	return 0;
    }
    if (offset % CNFS_BLOCKSIZE != 0) {
	SMseterror(SMERR_INTERNAL, NULL);
	syslog(L_ERROR,
	       "%s: CNFSsetusedbitbyrp: offset %s not on %d-byte block boundary",
	       LocalLogName, CNFSofft2hex(offset, FALSE), CNFS_BLOCKSIZE);
	return 0;
    }
    blocknum = offset / CNFS_BLOCKSIZE;
    longoffset = blocknum / (longsize * 8);
    bitoffset = blocknum % (longsize * 8);
    bitlong = *((ULONG *) cycbuff->bitfield + (CNFS_BEFOREBITF / longsize)
		+ longoffset);
    if (set_operation) {
	if (setbitvalue) {
	    mask = onarray[bitoffset];
	    bitlong |= mask;
	} else {
	    mask = offarray[bitoffset];
	    bitlong &= mask;
	}
	*((ULONG *) cycbuff->bitfield + (CNFS_BEFOREBITF / longsize)
	  + longoffset) = bitlong;
	return 2;	/* XXX Clean up return semantics */
    }
    /* It's a read operation */
    mask = onarray[bitoffset];
    return bitlong & mask;
}

/*
** CNFSmunmapbitfields() -- Call munmap() on all of the bitfields we've
**	previously mmap()'ed.
*/

STATIC void CNFSmunmapbitfields(void) {
    CYCBUFF	*cycbuff;

    for (cycbuff = cycbufftab; cycbuff != (CYCBUFF *)NULL; cycbuff = cycbuff->next) {
	if (cycbuff->bitfield != NULL) {
	    munmap(cycbuff->bitfield, cycbuff->minartoffset);
	    cycbuff->bitfield = NULL;
	}
    }
}

STATIC int CNFSArtMayBeHere(CYCBUFF *cycbuff, CYCBUFF_OFF_T offset, U_INT32_T cycnum) {
    static	count = 0;
    CYCBUFF	*tmp;

    if (++count % 1000 == 0) {	/* XXX 1K articles is just a guess */
	for (tmp = cycbufftab; tmp != (CYCBUFF *)NULL; tmp = tmp->next) {
	    CNFSReadFreeAndCycle(tmp);
	}
    }
    /*
    ** The current cycle number may have advanced since the last time we
    ** checked it, so use a ">=" check instead of "==".  Our intent is
    ** avoid a false negative response, *not* a false positive response.
    */
    if (! (cycnum == cycbuff->cyclenum ||
	(cycbuff->cyclenum == 0 && cycnum + 1 == cycbuff->cyclenum) ||
	(cycnum == cycbuff->cyclenum - 1 && offset > cycbuff->free) ||
	(cycnum == 0 && cycbuff->cyclenum == 2 && offset > cycbuff->free))) {
	/* We've been overwritten */
	return 0;
    }
    return CNFSUsedBlock(cycbuff, offset, FALSE, FALSE);
}

BOOL cnfs_init(void) {
    int			ret;

    if (innconf == NULL) {
	if ((ret = ReadInnConf(_PATH_CONFIG)) < 0) {
	    syslog(L_ERROR, "%s: ReadInnConf failed, returned %d", LocalLogName, ret);
	    SMseterror(SMERR_INTERNAL, "ReadInnConf() failed");
	    return FALSE;
	}
    }
    if (pagesize == 0) {
#ifdef  XXX
	if ((pagesize = sysconf(_SC_PAGESIZE)) < 0) {
	    syslog(L_ERROR, "%s: sysconf(_SC_PAGESIZE) failed: %m", LocalLogNam);
	    SMseterror(SMERR_INTERNAL, "sysconf(_SC_PAGESIZE) failed");
	    return NULL;
	}
#else   /* XXX */
	pagesize = 16384;	/* XXX Need comprehensive, portable solution */
#endif  /* XXX */
    }
    if (STORAGE_TOKEN_LENGTH < 16) {
	syslog(L_ERROR, "%s: token length is less than 16 bytes", LocalLogName);
	SMseterror(SMERR_TOKENSHORT, NULL);
	return FALSE;
    }

    if (!CNFSread_config()) {
	CNFScleancycbuff();
	CNFScleanmetacycbuff();
	CNFScleanexpirerule();
	return FALSE;
    }
    if (!CNFSinit_disks()) {
	CNFScleancycbuff();
	CNFScleanmetacycbuff();
	CNFScleanexpirerule();
	return FALSE;
    }

    return TRUE;
}

TOKEN cnfs_store(const ARTHANDLE article, STORAGECLASS class) {
    TOKEN               token;
    char		*p;
    CYCBUFF		*cycbuff = NULL;
    METACYCBUFF		*metacycbuff = NULL;
    int			i;
    static char		buf[1024], *bufp = buf;
    int			chars = 0;
    char		*artcycbuffname;
    CYCBUFF_OFF_T	artoffset, middle;
    U_INT32_T		artcyclenum;
    CNFSARTHEADER	cah;
    struct iovec	iov[2];
    int			tonextblock;
    CNFSEXPIRERULES	*metaexprule;

    for (metaexprule = metaexprulestab; metaexprule != (CNFSEXPIRERULES *)NULL; metaexprule = metaexprule->next) {
	if (metaexprule->class == class)
	    break;
    }
    if (metaexprule == (CNFSEXPIRERULES *)NULL) {
	SMseterror(SMERR_INTERNAL, "no rules match");
	syslog(L_ERROR, "%s: no matches for group '%s'",
	       LocalLogName, buf);
	token.type = TOKEN_EMPTY;
	return token;
    }
    metacycbuff = metaexprule->dest;

    cycbuff = metacycbuff->members[metacycbuff->memb_next];  
    /* Article too big? */
    if (cycbuff->free + article.len > cycbuff->len - CNFS_BLOCKSIZE - 1) {
	for (middle = cycbuff->free ;middle < cycbuff->len - CNFS_BLOCKSIZE - 1;
	    middle += CNFS_BLOCKSIZE) {
	    CNFSUsedBlock(cycbuff, middle, TRUE, FALSE);
	}
	cycbuff->free = cycbuff->minartoffset;
	cycbuff->cyclenum++;
	if (cycbuff->cyclenum == 1)
	    cycbuff->cyclenum++;		/* cnfs_next() needs this */
	cycbuff->needflush = TRUE;
	(void)CNFSflushhead(cycbuff);		/* Flush, just for giggles */
	syslog(L_NOTICE, "%s: cycbuff %s rollover to cycle 0x%x... remain calm",
	       LocalLogName, cycbuff->name, cycbuff->cyclenum);
    }
    /* Ah, at least we know all three important data */
    artcycbuffname = cycbuff->name;
    artoffset = cycbuff->free;
    artcyclenum = cycbuff->cyclenum;

    memset(&cah, 0, sizeof(cah));
    cah.size = htonl(article.len);
    if (article.arrived == (time_t)0)
	cah.arrived = htonl(time(NULL));
    else
	cah.arrived = htonl(article.arrived);
    cah.class = htonl(class);

    if (CNFSseek(cycbuff->fdrdwr, artoffset, SEEK_SET) < 0) {
	SMseterror(SMERR_INTERNAL, "CNFSseek() failed");
	syslog(L_ERROR, "%s: CNFSseek failed for '%s' offset 0x%s: %m",
	       LocalLogName, cycbuff->name, CNFSofft2hex(artoffset, FALSE));
	token.type = TOKEN_EMPTY;
	return token;
    }
    iov[0].iov_base = (caddr_t) &cah;
    iov[0].iov_len = sizeof(cah);
    iov[1].iov_base = article.data;
    iov[1].iov_len = article.len;
    if (xwritev(cycbuff->fdrdwr, iov, 2) < 0) {
	SMseterror(SMERR_INTERNAL, "cnfs_store() xwritev() failed");
	syslog(L_ERROR,
	       "%s: cnfs_store xwritev failed for '%s' offset 0x%s: %m",
	       LocalLogName, artcycbuffname, CNFSofft2hex(artoffset, FALSE));
	token.type = TOKEN_EMPTY;
	return token;
    }
    cycbuff->needflush = TRUE;

    /* Now that the article is written, advance the free pointer & flush */
    cycbuff->free += (CYCBUFF_OFF_T) article.len + sizeof(cah);
    tonextblock = CNFS_BLOCKSIZE - (cycbuff->free & (CNFS_BLOCKSIZE - 1));
    cycbuff->free += (CYCBUFF_OFF_T) tonextblock;
    /*
    ** If cycbuff->free > cycbuff->len, don't worry.  The next cnfs_store()
    ** will detect the situation & wrap around correctly.
    */
    metacycbuff->memb_next = (metacycbuff->memb_next + 1) % metacycbuff->count;
    if (++metacycbuff->write_count % METACYCBUFF_UPDATE == 0) {
	for (i = 0; i < metacycbuff->count; i++) {
	    (void)CNFSflushhead(metacycbuff->members[i]);
	}
    }
    if (metacycbuff->write_count % 1000000 == 0) {
	syslog(L_NOTICE,
	       "%s: cnfs_store metacycbuff %s just wrote its %ld'th article",
	       LocalLogName, metacycbuff->name, metacycbuff->write_count);
    }
    /* is this stuff needed? */
    CNFSUsedBlock(cycbuff, artoffset, TRUE, TRUE);
    for (middle = artoffset + CNFS_BLOCKSIZE; middle < cycbuff->free;
	 middle += CNFS_BLOCKSIZE) {
	CNFSUsedBlock(cycbuff, middle, TRUE, FALSE);
    }
    return CNFSMakeToken(artcycbuffname, artoffset, artcyclenum, class);
}

ARTHANDLE *cnfs_retrieve(const TOKEN token, RETRTYPE amount) {
    char		cycbuffname[9];
    CYCBUFF_OFF_T	offset;
    U_INT32_T		cycnum;
    CYCBUFF		*cycbuff;
    ARTHANDLE   	*art;
    CNFSARTHEADER	cah;
    PRIV_CNFS		*private;
    char		*p;
    long		pagefudge;
    CYCBUFF_OFF_T	mmapoffset;
    static TOKEN	ret_token;

    if (token.type != TOKEN_CNFS) {
	SMseterror(SMERR_INTERNAL, NULL);
	return NULL;
    }
    if (! CNFSBreakToken(token, cycbuffname, &offset, &cycnum)) {
	/* SMseterror() should have already been called */
	return NULL;
    }
    if ((cycbuff = CNFSgetcycbuffbyname(cycbuffname)) == NULL) {
	SMseterror(SMERR_INTERNAL, "bogus cycbuff name");
	syslog(L_ERROR, "%s: cnfs_retrieve: token %s: bogus cycbuff name: %s:0x%s:%ld",
	       LocalLogName, TokenToText(token), cycbuffname, CNFSofft2hex(offset, FALSE), cycnum);
	return NULL;
    }
    if (! CNFSArtMayBeHere(cycbuff, offset, cycnum)) {
	SMseterror(SMERR_NOENT, NULL);
	return NULL;
    }

    art = NEW(ARTHANDLE, 1);
    art->type = TOKEN_CNFS;
    if (amount == RETR_STAT) {
	art->data = NULL;
	art->len = 0;
	art->private = NULL;
	return art;
    }
    /*
    ** Because we don't know the length of the article (yet), we'll
    ** just mmap() a chunk of memory which is guaranteed to be larger
    ** than the largest article can be.
    ** XXX Because the max article size can be changed, we could get into hot
    ** XXX water here.  So, to be safe, we double MAX_ART_SIZE and add enough
    ** XXX extra for the pagesize fudge factor and CNFSARTHEADER structure.
    */
    if (CNFSseek(cycbuff->fdrd, offset, SEEK_SET) < 0) {
        SMseterror(SMERR_UNDEFINED, "CNFSseek failed");
        syslog(L_ERROR, "%s: could not lseek token %s %s:0x%s:%ld: %m",
		LocalLogName, TokenToText(token), cycbuffname, CNFSofft2hex(offset, FALSE), cycnum);
        DISPOSE(art->private);
        DISPOSE(art);
        return NULL;
    }
    if (read(cycbuff->fdrd, &cah, sizeof(cah)) != sizeof(cah)) {
        SMseterror(SMERR_UNDEFINED, "read failed");
        syslog(L_ERROR, "%s: could not read token %s %s:0x%s:%ld: %m",
		LocalLogName, TokenToText(token), cycbuffname, CNFSofft2hex(offset, FALSE), cycnum);
        DISPOSE(art->private);
        DISPOSE(art);
        return NULL;
    }
    private = NEW(PRIV_CNFS, 1);
    art->private = (void *)private;
    art->arrived = cah.arrived;
    offset += sizeof(cah);
    pagefudge = offset % pagesize;
    mmapoffset = offset - pagefudge;
    private->len = pagefudge + cah.size;
    if ((private->base = mmap((MMAP_PTR)0, private->len, PROT_READ, MAP__ARG,
			      cycbuff->fdrd, mmapoffset)) == (MMAP_PTR) -1) {
        SMseterror(SMERR_UNDEFINED, "mmap failed");
        syslog(L_ERROR, "%s: could not mmap token %s %s:0x%s:%ld: %m",
	        LocalLogName, TokenToText(token), cycbuffname, CNFSofft2hex(offset, FALSE), cycnum);
        DISPOSE(art->private);
        DISPOSE(art);
        return NULL;
    }
    ret_token = token;    
    art->token = &ret_token;
    if (amount == RETR_ALL) {
	art->data = private->base + pagefudge;
	art->len = cah.size;
	return art;
    }
    if ((p = SMFindBody(private->base + pagefudge, art->len)) == NULL) {
        SMseterror(SMERR_NOBODY, NULL);
	munmap(private->base, private->len);
        DISPOSE(art->private);
        DISPOSE(art);
        return NULL;
    }
    if (amount == RETR_HEAD) {
	art->data = private->base + pagefudge;
        art->len = p - private->base + pagefudge;
        return art;
    }
    if (amount == RETR_BODY) {
        art->data = p + 4;
        art->len = art->len - (private->base + pagefudge - p - 4);
        return art;
    }
    SMseterror(SMERR_UNDEFINED, "Invalid retrieve request");
    munmap(private->base, private->len);
    DISPOSE(art->private);
    DISPOSE(art);
    return NULL;
}

void cnfs_freearticle(ARTHANDLE *article) {
    PRIV_CNFS	*private;

    if (!article)
	return;
    
    if (article->private) {
	private = (PRIV_CNFS *)article->private;
#if defined(MADV_DONTNEED) && !defined(_nec_ews)
	madvise(private->base, private->len, MADV_DONTNEED);
#endif	/* MADV_DONTNEED */
	munmap(private->base, private->len);
	DISPOSE(private);
    }
    DISPOSE(article);
}

BOOL cnfs_cancel(TOKEN token) {
    return TRUE;
}

ARTHANDLE *cnfs_next(const ARTHANDLE *article, RETRTYPE amount) {
    ARTHANDLE           *art;
    CYCBUFF		*cycbuff;
    PRIV_CNFS		priv, *private;
    CYCBUFF_OFF_T	middle;
    CNFSARTHEADER	cah;
    CYCBUFF_OFF_T	offset;
    long		pagefudge;
    static TOKEN	token;
    int			tonextblock;
    CYCBUFF_OFF_T	mmapoffset;
    char		*p;

    if (article == (ARTHANDLE *)NULL) {
	if ((cycbuff = cycbufftab) == (CYCBUFF *)NULL)
	    return (ARTHANDLE *)NULL;
	priv.offset = 0;
    } else {        
	priv = *(PRIV_CNFS *)article->private;
	DISPOSE(article->private);
	DISPOSE(article);
#if defined(MADV_DONTNEED) && !defined(_nec_ews)
	madvise(priv.base, priv.len, MADV_DONTNEED);  
#endif
	munmap(priv.base, priv.len);
	cycbuff = priv.cycbuff;
    }

    for (;cycbuff != (CYCBUFF *)NULL; cycbuff = cycbuff->next) {
	if (priv.rollover && priv.offset >= cycbuff->free) {
	    priv.offset = 0;
	    continue;
	}
	if (priv.offset == 0) {
	    priv.offset = cycbuff->minartoffset;
	    if (cycbuff->cyclenum == 1)
		priv.rollover = TRUE;
	    else
		priv.rollover = FALSE;
	}
	if (!priv.rollover) {
	    for (middle = priv.offset ;middle < cycbuff->len - CNFS_BLOCKSIZE - 1;
		middle += CNFS_BLOCKSIZE) {
		if (CNFSUsedBlock(cycbuff, middle, FALSE, FALSE) == 0)
		    continue;
	    }
	    if (middle >= cycbuff->len - CNFS_BLOCKSIZE - 1) {
		priv.rollover = TRUE;
		priv.offset = cycbuff->minartoffset;
	    } else
		break;
	}
	if (priv.rollover) {
	    for (middle = cycbuff->minartoffset ;middle < cycbuff->free;
		middle += CNFS_BLOCKSIZE) {
		if (CNFSUsedBlock(cycbuff, middle, FALSE, FALSE) == 0)
		    continue;
	    }
	    if (middle >= cycbuff->free) {
		priv.offset = 0;
		continue;
	    } else
		break;
	}
    }
    if (cycbuff == (CYCBUFF *)NULL)
	return (ARTHANDLE *)NULL;

    offset = priv.offset;
    if (CNFSseek(cycbuff->fdrd, offset, SEEK_SET) < 0) {
	return (ARTHANDLE *)NULL;
    }
    if (read(cycbuff->fdrd, &cah, sizeof(cah)) != sizeof(cah)) {
	return (ARTHANDLE *)NULL;
    }
    art = NEW(ARTHANDLE, 1);
    private = NEW(PRIV_CNFS, 1);
    art->private = (void *)private;
    art->type = TOKEN_CNFS;
    priv.offset += (CYCBUFF_OFF_T) cah.size + sizeof(cah);
    tonextblock = CNFS_BLOCKSIZE - (priv.offset & (CNFS_BLOCKSIZE - 1));
    priv.offset += (CYCBUFF_OFF_T) tonextblock;
    art->arrived = cah.arrived;
    token = CNFSMakeToken(cycbuff->name, offset, cycbuff->cyclenum, cah.class);
    art->token = &token;
    offset += sizeof(cah);
    pagefudge = offset % pagesize;
    mmapoffset = offset - pagefudge;
    private->len = pagefudge + cah.size;
    if ((private->base = mmap((MMAP_PTR)0, private->len, PROT_READ, MAP__ARG,
	cycbuff->fdrd, mmapoffset)) == (MMAP_PTR) -1) {
	art->data = NULL;
	art->len = 0;
	return art;
    }
    if (amount == RETR_ALL) {
	art->data = private->base + pagefudge;
	art->len = cah.size;
	return art;
    }
    if ((p = SMFindBody(private->base + pagefudge, art->len)) == NULL) {
	art->data = NULL;
	art->len = 0;
	return art;
    }
    if (amount == RETR_HEAD) {
	art->data = private->base + pagefudge;
	art->len = p - private->base + pagefudge;
	return art;
    }
    if (amount == RETR_BODY) {
	art->data = p + 4;
	art->len = art->len - (private->base + pagefudge - p - 4);
	return art;
    }
    art->data = NULL;
    art->len = 0;
    return art;
}

void cnfs_shutdown(void) {
    CNFSflushallheads();
    CNFSmunmapbitfields();
    CNFScleancycbuff();
    CNFScleanmetacycbuff();
    CNFScleanexpirerule();
}
