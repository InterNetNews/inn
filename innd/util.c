/*  $Id$
**
**  Various miscellaneous utility functions for innd internal use.
*/

#include "config.h"
#include "clibrary.h"

#include "inn/innconf.h"
#include "libinn.h"

#include "innd.h"

/*
**  Sprintf a long into a buffer with enough leading zero's so that it
**  takes up width characters.  Don't add trailing NUL.  Return TRUE
**  if it fit.  Used for updating high-water marks in the active file
**  in-place.
*/
bool
FormatLong(char *p, unsigned long value, int width)
{
    for (p += width - 1; width-- > 0; ) {
        *p-- = (int)(value % 10) + '0';
        value /= 10;
    }
    return value == 0;
}


/*
**  Glue a string, a char, and a string together.  Useful for making
**  filenames.
*/
void
FileGlue(char *p, const char *n1, char c,
         const char *n2)
{
    p += strlen(strcpy(p, n1));
    *p++ = c;
    strcpy(p, n2);
}


/*
**  Turn any \r or \n in text into spaces.  Used to splice back multi-line
**  headers into a single line.
*/
static char *
Join(char *text)
{
    char       *p;

    for (p = text; *p; p++)
        if (*p == '\n' || *p == '\r')
            *p = ' ';
    return text;
}


/*
**  Return a short name that won't overrun our bufer or syslog's buffer.
**  q should either be p, or point into p where the "interesting" part is.
*/
char *
MaxLength(const char *p, const char *q)
{
    static char buff[80];
    unsigned int i;

    /* Already short enough? */
    i = strlen(p);
    if (i < sizeof buff - 1) {
        strcpy(buff, p);
        return Join(buff);
    }

    /* Simple case of just want the begining? */
    if ((unsigned)(q - p) < sizeof buff - 4) {
        strncpy(buff, p, sizeof buff - 4);
        strcpy(&buff[sizeof buff - 4], "...");
    }
    /* Is getting last 10 characters good enough? */
    else if ((p + i) - q < 10) {
        strncpy(buff, p, sizeof buff - 14);
        strcpy(&buff[sizeof buff - 14], "...");
        strcpy(&buff[sizeof buff - 11], &p[i - 10]);
    }
    else {
        /* Not in last 10 bytes, so use double elipses. */
        strncpy(buff, p, sizeof buff - 17);
        strcpy(&buff[sizeof buff - 17], "...");
        strncpy(&buff[sizeof buff - 14], &q[-5], 10);
        strcpy(&buff[sizeof buff - 4], "...");
    }
    return Join(buff);
}


/*
**  Split text into comma-separated fields.  Return an allocated
**  NULL-terminated array of the fields within the modified argument that
**  the caller is expected to save or free.  We don't use strchr() since
**  the text is expected to be either relatively short or "comma-dense."
*/
char **
CommaSplit(char *text)
{
    int i;
    char *p;
    char **av;
    char **save;

    /* How much space do we need? */
    for (i = 2, p = text; *p; p++)
        if (*p == ',')
            i++;

    for (av = save = NEW(char*, i), *av++ = p = text; *p; )
        if (*p == ',') {
            *p++ = '\0';
            *av++ = p;
        }
        else
            p++;
    *av = NULL;
    return save;
}


/*
**  Set up LISTBUFFER so that data will be put into array
**  it allocates buffer and array for data if needed, otherwise use already
**  allocated one
*/
void
SetupListBuffer(int size, LISTBUFFER *list)
{
  /* get space for data to be splitted */
  if (list->Data == NULL) {
    list->DataLength = size;
    list->Data = NEW(char, list->DataLength + 1);
  } else if (list->DataLength < size) {
    list->DataLength = size;
    RENEW(list->Data, char, list->DataLength + 1);
  }
  /* get an array of character pointers. */
  if (list->List == NULL) {
    list->ListLength = DEFAULTNGBOXSIZE;
    list->List = NEW(char*, list->ListLength);
  }
}


/*
**  Do we need a shell for the command?  If not, av is filled in with
**  the individual words of the command and the command is modified to
**  have NUL's inserted.
*/
bool
NeedShell(char *p, const char **av, const char **end)
{
    static const char Metachars[] = ";<>|*?[]{}()#$&=`'\"\\~\n";
    const char *q;

    /* We don't use execvp(); works for users, fails out of /etc/rc. */
    if (*p != '/')
        return TRUE;
    for (q = p; *q; q++)
        if (strchr(Metachars, *q) != NULL)
            return TRUE;

    for (end--; av < end; ) {
        /* Mark this word, check for shell meta-characters. */
        for (*av++ = p; *p && !ISWHITE(*p); p++)
            continue;

        /* If end of list, we're done. */
        if (*p == '\0') {
            *av = NULL;
            return FALSE;
        }

        /* Skip whitespace, find next word. */
        for (*p++ = '\0'; ISWHITE(*p); p++)
            continue;
        if (*p == '\0') {
            *av = NULL;
            return FALSE;
        }
    }

    /* Didn't fit. */
    return TRUE;
}


/*
**  Spawn a process, with I/O redirected as needed.  Return the PID or -1
**  (and a syslog'd message) on error.
*/
pid_t
Spawn(int niceval, int fd0, int fd1, int fd2, char * const av[])
{
    static char NOCLOSE[] = "%s cant close %d in %s %m";
    static char NODUP2[] = "%s cant dup2 %d to %d in %s %m";
    pid_t       i;

    /* Fork; on error, give up.  If not using the patched dbz, make
     * this call fork! */
    i = fork();
    if (i == -1) {
        syslog(L_ERROR, "%s cant fork %s %m", LogName, av[0]);
        return -1;
    }

    /* If parent, do nothing. */
    if (i > 0)
        return i;

    /* Child -- do any I/O redirection. */
    if (fd0 != 0) {
        if (dup2(fd0, 0) < 0) {
            syslog(L_FATAL, NODUP2, LogName, fd0, 0, av[0]);
            _exit(1);
        }
        if (fd0 != fd1 && fd0 != fd2 && close(fd0) < 0)
            syslog(L_ERROR, NOCLOSE, LogName, fd0, av[0]);
    }
    if (fd1 != 1) {
        if (dup2(fd1, 1) < 0) {
            syslog(L_FATAL, NODUP2, LogName, fd1, 1, av[0]);
            _exit(1);
        }
        if (fd1 != fd2 && close(fd1) < 0)
            syslog(L_ERROR, NOCLOSE, LogName, fd1, av[0]);
    }
    if (fd2 != 2) {
        if (dup2(fd2, 2) < 0) {
            syslog(L_FATAL, NODUP2, LogName, fd2, 2, av[0]);
            _exit(1);
        }
        if (close(fd2) < 0)
            syslog(L_ERROR, NOCLOSE, LogName, fd2, av[0]);
    }
    close_on_exec(0, false);
    close_on_exec(1, false);
    close_on_exec(2, false);

    /* Nice our child if we're supposed to. */
    if (niceval != 0 && nice(niceval) == -1)
        syslog(L_ERROR, "SERVER cant nice child to %d: %m", niceval);

    /* Start the desired process (finally!). */
    execv(av[0], av);
    syslog(L_FATAL, "%s cant exec in %s %m", LogName, av[0]);
    _exit(1);

    /* Not reached. */
    return -1;
}

/*
**  We ran out of space or other I/O error, throttle ourselves.
*/
void
ThrottleIOError(const char *when)
{
    char         buff[SMBUF];
    const char * p;
    int          oerrno;

    if (Mode == OMrunning) {
        oerrno = errno;
        if (Reservation) {
            DISPOSE(Reservation);
            Reservation = NULL;
        }
        snprintf(buff, sizeof(buff), "%s writing %s file -- throttling",
                 strerror(oerrno), when);
        if ((p = CCblock(OMthrottled, buff)) != NULL)
            syslog(L_ERROR, "%s cant throttle %s", LogName, p);
        syslog(L_FATAL, "%s throttle %s", LogName, buff);
        errno = oerrno;
        ThrottledbyIOError = TRUE;
    }
}

/*
**  No matching storage.conf, throttle ourselves.
*/
void
ThrottleNoMatchError(void)
{
    char buff[SMBUF];
    const char *p;

    if (Mode == OMrunning) {
        if (Reservation) {
            DISPOSE(Reservation);
            Reservation = NULL;
        }
        snprintf(buff, sizeof(buff), "%s storing article -- throttling",
                 SMerrorstr);
        if ((p = CCblock(OMthrottled, buff)) != NULL)
            syslog(L_ERROR, "%s cant throttle %s", LogName, p);
        syslog(L_FATAL, "%s throttle %s", LogName, buff);
        ThrottledbyIOError = TRUE;
    }
}

void
InndHisOpen(void)
{
    char *histpath;
    int flags;
    size_t synccount;

    histpath = concatpath(innconf->pathdb, _PATH_HISTORY);
    if (innconf->hismethod == NULL) {
	sysdie("hismethod is not defined");
	/*NOTREACHED*/
    }

    flags = HIS_RDWR | (INND_DBZINCORE ? HIS_MMAP : HIS_ONDISK);
    History = HISopen(histpath, innconf->hismethod, flags);
    if (!History) {
	sysdie("SERVER can't open history %s", histpath);
	/*NOTREACHED*/
    }
    free(histpath);
    HISsetcache(History, 1024 * innconf->hiscachesize);
    synccount = innconf->icdsynccount;
    HISctl(History, HISCTLS_SYNCCOUNT, &synccount);
}

void
InndHisClose(void)
{
    if (History == NULL)
        return;
    if (!HISclose(History)) {
        char *histpath;

	histpath = concatpath(innconf->pathdb, _PATH_HISTORY);
	sysdie("SERVER can't close history %s", histpath);
	free(histpath);
    }	
    History = NULL;
}

bool
InndHisWrite(const char *key, time_t arrived, time_t posted, time_t expires,
	     TOKEN *token)
{
    bool r = HISwrite(History, key, arrived, posted, expires, token);

    if (r != true)
	IOError("history write", errno);
    return r;
}

bool
InndHisRemember(const char *key)
{
    bool r = HISremember(History, key, Now.time);

    if (r != true)
	IOError("history remember", errno);
    return r;
}

void
InndHisLogStats(void)
{
    struct histstats stats = HISstats(History);

    syslog(L_NOTICE, "ME HISstats %d hitpos %d hitneg %d missed %d dne",
	   stats.hitpos, stats.hitneg, stats.misses, stats.dne);
}


