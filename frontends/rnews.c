/*  $Id$
**
**  A front-end for InterNetNews.
**
**  Read UUCP batches and offer them up NNTP-style.  Because we may end
**  up sending our input down a pipe to uncompress, we have to be careful
**  to do unbuffered reads.
*/

#include "config.h"
#include "clibrary.h"
#include "portable/wait.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <syslog.h>
#include <sys/stat.h>

#include "libinn.h"
#include "macros.h"
#include "nntp.h"
#include "paths.h"


typedef struct _HEADER {
    const char *Name;
    int size;
} HEADER;


static bool	Verbose;
static const char	*InputFile = "stdin";
static char	*UUCPHost;
static char	*PathBadNews = NULL;
static char	*remoteServer;
static FILE	*FromServer;
static FILE	*ToServer;
static char	UNPACK[] = "gzip";
static HEADER	RequiredHeaders[] = {
    { "Message-ID",	10 },
#define _messageid	0
    { "Newsgroups",	10 },
#define _newsgroups	1
    { "From",		 4 },
#define _from		2
    { "Date",		 4 },
#define _date		3
    { "Subject",	 7 },
#define _subject	4
    { "Path",		 4 },
#define _path		5
};
#define IS_MESGID(hp)	((hp) == &RequiredHeaders[_messageid])
#define IS_PATH(hp)	((hp) == &RequiredHeaders[_path])



/*
**  Do perror, making sure errno is preserved.
*/
static void
xperror(const char *p)
{
    int		oerrno;

    oerrno = errno;
    perror(p);
    errno = oerrno;
}


/*
**  Open up a pipe to a process with fd tied to its stdin.  Return a
**  descriptor tied to its stdout or -1 on error.
*/
static int
StartChild(int fd, const char *path, const char *argv[])
{
    int		pan[2];
    int		i;
    pid_t	pid;

    /* Create a pipe. */
    if (pipe(pan) < 0) {
	syslog(L_FATAL, "cant pipe for %s %m", path);
	exit(1);
    }

    /* Get a child. */
    for (i = 0; (pid = fork()) < 0; i++) {
	if (i == innconf->maxforks) {
	    syslog(L_ERROR, "cant fork %s %m -- spooling", path);
	    return -1;
	}
	syslog(L_NOTICE, "cant fork %s -- waiting", path);
	(void)sleep(60);
    }

    /* Run the child, with redirection. */
    if (pid == 0) {
	(void)close(pan[PIPE_READ]);

	/* Stdin comes from our old input. */
	if (fd != STDIN_FILENO) {
	    if ((i = dup2(fd, STDIN_FILENO)) != STDIN_FILENO) {
		syslog(L_FATAL, "cant dup2 %d to 0 got %d %m", fd, i);
		_exit(1);
	    }
	    (void)close(fd);
	}

	/* Stdout goes down the pipe. */
	if (pan[PIPE_WRITE] != STDOUT_FILENO) {
	    if ((i = dup2(pan[PIPE_WRITE], STDOUT_FILENO)) != STDOUT_FILENO) {
		syslog(L_FATAL, "cant dup2 %d to 1 got %d %m",
		    pan[PIPE_WRITE], i);
		_exit(1);
	    }
	    (void)close(pan[PIPE_WRITE]);
	}

	(void)execv(path, argv);
	syslog(L_FATAL, "cant execv %s %m", path);
	_exit(1);
    }

    (void)close(pan[PIPE_WRITE]);
    (void)close(fd);
    return pan[PIPE_READ];
}


/*
**  Wait for the specified number of children.
*/
static void
WaitForChildren(int n)
{
    pid_t pid;

    while (--n >= 0) {
        pid = waitpid(-1, NULL, WNOHANG);
        if (pid == (pid_t) -1 && errno != EINTR) {
            if (errno != ECHILD)
                syslog(L_ERROR, "cant wait %m");
            break;
        }
    }
}




/*
**  Clean up the NNTP escapes from a line.
*/
static char *REMclean(char *buff)
{
    char	*p;

    if ((p = strchr(buff, '\r')) != NULL)
	*p = '\0';
    if ((p = strchr(buff, '\n')) != NULL)
	*p = '\0';

    /* The dot-escape is only in text, not command responses. */
    return buff;
}


/*
**  Write an article to the rejected directory.
*/
static void Reject(const char *article, const char *reason, const char *arg)
{
#if	defined(DO_RNEWS_SAVE_BAD)
    char *filename;
    FILE *F;
    int fd;
    size_t length;
#endif	/* defined(DO_RNEWS_SAVE_BAD) */

    syslog(L_NOTICE, reason, arg);
    if (Verbose) {
	(void)fprintf(stderr, "%s: ", InputFile);
	(void)fprintf(stderr, reason, arg);
	(void)fprintf(stderr, " [%.40s...]\n", article);
    }

#if	defined(DO_RNEWS_SAVE_BAD)
    filename = concat(PathBadNews, "/XXXXXX", (char *) 0);
    fd = mkstemp(filename);
    if (fd < 0) {
        syslog(L_ERROR, "cant create temporary file %m");
        return;
    }
    F = fdopen(fd, "w");
    if (F == NULL) {
	syslog(L_ERROR, "cant fdopen %s %m", filename);
	return;
    }
    length = strlen(article);
    if (fwrite(article, 1, length, F) != length)
	syslog(L_ERROR, "cant fwrite %s %m", filename);
    if (fclose(F) == EOF)
	syslog(L_ERROR, "cant close %s %m", filename);
    free(filename);
#endif	/* defined(DO_RNEWS_SAVE_BAD) */
}


/*
**  Process one article.  Return TRUE if the article was okay; FALSE if the
**  whole batch needs to be saved (such as when the server goes down or if
**  the file is corrupted).
*/
static bool Process(char *article)
{
    HEADER	        *hp;
    const char	        *p;
    char                *q;
    const char		*id = NULL;
    char                *msgid;
    char		buff[SMBUF];
#if	defined(FILE_RNEWS_LOG_DUPS)
    FILE		*F;
#endif	/* defined(FILE_RNEWS_LOG_DUPS) */
#if	!defined(DONT_RNEWS_LOG_DUPS)
    char		path[40];
#endif	/* !defined(DONT_RNEWS_LOG_DUPS) */

    /* Empty article? */
    if (*article == '\0')
	return TRUE;

    /* Make sure that all the headers are there, note the ID. */
    for (hp = RequiredHeaders; hp < ENDOF(RequiredHeaders); hp++) {
	if ((p = HeaderFindMem(article, strlen(article), hp->Name, hp->size)) == NULL) {
	    Reject(article, "bad_article missing %s", hp->Name);
	    return FALSE;
	}
	if (IS_MESGID(hp)) {
	    id = p;
	    continue;
	}
#if	!defined(DONT_RNEWS_LOG_DUPS)
	if (IS_PATH(hp)) {
	    (void)strncpy(path, p, sizeof path);
	    path[sizeof path - 1] = '\0';
	    if ((q = strchr(path, '\n')) != NULL)
		*q = '\0';
	}
#endif	/* !defined(DONT_RNEWS_LOG_DUPS) */
    }

    /* Send the NNTP "ihave" message. */
    if ((p = strchr(id, '\n')) == NULL) {
	Reject(article, "bad_article unterminated %s header", "Message-ID");
	return FALSE;
    }
    msgid = xstrndup(id, p - id);
    fprintf(ToServer, "ihave %s\r\n", msgid);
    fflush(ToServer);
    if (UUCPHost)
	syslog(L_NOTICE, "offered %s %s", msgid, UUCPHost);
    free(msgid);

    /* Get a reply, see if they want the article. */
    if (fgets(buff, sizeof buff, FromServer) == NULL) {
	syslog(L_ERROR, "cant fgets after ihave %m");
	return FALSE;
    }
    (void)REMclean(buff);
    if (!CTYPE(isdigit, buff[0])) {
	syslog(L_NOTICE, "bad_reply after ihave %s", buff);
	return FALSE;
    }
    switch (atoi(buff)) {
    default:
	Reject(article, "unknown_reply after ihave %s", buff);
	return TRUE;
    case NNTP_RESENDIT_VAL:
	return FALSE;
    case NNTP_SENDIT_VAL:
	break;
    case NNTP_HAVEIT_VAL:
#if	defined(SYSLOG_RNEWS_LOG_DUPS)
	*p = '\0';
	syslog(L_NOTICE, "duplicate %s %s", id, path);
#endif	/* defined(SYSLOG_RNEWS_LOG_DUPS) */
#if	defined(FILE_RNEWS_LOG_DUPS)
	if ((F = fopen(_PATH_RNEWS_DUP_LOG, "a")) != NULL) {
	    *p = '\0';
	    (void)fprintf(F, "duplicate %s %s\n", id, path);
	    (void)fclose(F);
	}
#endif	/* defined(FILE_RNEWS_LOG_DUPS) */
	return TRUE;
    }

    /* Send all the lines in the article, escaping periods. */
    if (NNTPsendarticle(article, ToServer, TRUE) < 0) {
	syslog(L_NOTICE, "cant sendarticle %m");
	return FALSE;
    }

    /* Process server reply code. */
    if (fgets(buff, sizeof buff, FromServer) == NULL) {
	syslog(L_ERROR, "cant fgets after article %m");
	return FALSE;
    }
    (void)REMclean(buff);
    if (!CTYPE(isdigit, buff[0])) {
	syslog(L_NOTICE, "bad_reply after article %s", buff);
	return FALSE;
    }
    switch (atoi(buff)) {
    default:
	syslog(L_NOTICE, "unknown_reply after article %s", buff);
	/* FALLTHROUGH */
    case NNTP_RESENDIT_VAL:
	return FALSE;
    case NNTP_TOOKIT_VAL:
	break;
    case NNTP_REJECTIT_VAL:
	Reject(article, "rejected %s", buff);
	break;
    }
    return TRUE;
}


/*
**  Read the rest of the input as an article.  Just punt to stdio in
**  this case and let it do the buffering.
*/
static bool
ReadRemainder(register int fd, char first, char second)
{
    register FILE	*F;
    register char	*article;
    register char       *p;
    register int	size;
    register int	used;
    register int	left;
    register int	i;
    bool		ok;

    /* Turn the descriptor into a stream. */
    if ((F = fdopen(fd, "r")) == NULL) {
	syslog(L_FATAL, "can't fdopen %d %m", fd);
	exit(1);
    }

    /* Get an initial allocation, leaving space for the \0. */
    size = BUFSIZ + 1;
    article = NEW(char, size + 2);
    article[0] = first;
    article[1] = second;
    used = second ? 2 : 1;
    left = size - used;

    /* Read the input.  Read a line at a time so that we can convert line
       endings if necessary. */
    p = fgets(article + used, left, F);
    while (p != NULL) {
	i = strlen(p);
        if (p[i-2] == '\r') {
            p[i-2] = '\n';
            p[i-1] = '\0';
        }
	used += i;
        left -= i;
        if (left < SMBUF) {
            size += BUFSIZ;
            left += BUFSIZ;
            RENEW(article, char, size);
        }
        p = fgets(article + used, left, F);
    }
    if (!feof(F)) {
        syslog(L_FATAL, "cant fgets after %d bytes %m", used);
        exit(1);
    }

    if (article[used - 1] != '\n')
	article[used++] = '\n';
    article[used] = '\0';
    (void)fclose(F);

    ok = Process(article);
    DISPOSE(article);
    return ok;
}


/*
**  Read an article from the input stream that is artsize bytes long.
*/
static bool
ReadBytecount(register int fd, int artsize)
{
    static char		*article;
    static int		oldsize;
    register char	*p;
    register int	left;
    register int	i;

    /* If we haven't gotten any memory before, or we didn't get enough,
     * then get some. */
    if (article == NULL) {
	oldsize = artsize;
	article = NEW(char, oldsize + 1 + 1);
    }
    else if (artsize > oldsize) {
	oldsize = artsize;
	RENEW(article, char, oldsize + 1 + 1);
    }

    /* Read in the article. */
    for (p = article, left = artsize; left; p += i, left -= i)
	if ((i = read(fd, p, left)) <= 0) {
	    i = errno;
	    syslog(L_ERROR, "cant read wanted %d got %d %m",
		artsize, artsize - left);
#if	0
	    /* Don't do this -- if the article gets re-processed we
	     * will end up accepting the truncated version. */
	    artsize = p - article;
	    article[artsize] = '\0';
	    Reject(article, "short read (%s?)", strerror(i));
#endif	/* 0 */
	    return TRUE;
	}
    if (p[-1] != '\n')
	*p++ = '\n';
    *p = '\0';

    return Process(article);
}



/*
**  Read a single text line; not unlike fgets().  Just more inefficient.
*/
static bool
ReadLine(char *p, int size, int fd)
{
    char	*save;

    /* Fill the buffer, a byte at a time. */
    for (save = p; size > 0; p++, size--) {
	if (read(fd, p, 1) != 1) {
	    *p = '\0';
	    syslog(L_FATAL, "cant read first line got %s %m", save);
	    exit(1);
	}
	if (*p == '\n') {
	    *p = '\0';
	    return TRUE;
	}
    }
    *p = '\0';
    syslog(L_FATAL, "bad_line too long %s", save);
    return FALSE;
}


/*
**  Unpack a single batch.
*/
static bool
UnpackOne(int *fdp, size_t *countp)
{
#if	defined(DO_RNEWSPROGS)
    char	path[(SMBUF * 2) + 1];
    char	*p;
#endif	/* defined(DO_RNEWSPROGS) */
    char	buff[SMBUF];
    const char *cargv[4];
    int		artsize;
    int		i;
    int		gzip = 0;
    bool	HadCount;
    bool	SawCunbatch;
    int		len;

    *countp = 0;
    for (SawCunbatch = FALSE, HadCount = FALSE; ; ) {
	/* Get the first character. */
	if ((i = read(*fdp, &buff[0], 1)) < 0) {
	    syslog(L_ERROR, "cant read first character %m");
	    return FALSE;
	}
	if (i == 0)
	    break;

	if (buff[0] == 0x1f)
	    gzip = 1;
	else if (buff[0] != RNEWS_MAGIC1)
	    /* Not a batch file.  If we already got one count, the batch
	     * is corrupted, else read rest of input as an article. */
	    return HadCount ? FALSE : ReadRemainder(*fdp, buff[0], '\0');

	/* Get the second character. */
	if ((i = read(*fdp, &buff[1], 1)) < 0) {
	    syslog(L_ERROR, "cant read second character %m");
	    return FALSE;
	}
	if (i == 0)
	    /* A one-byte batch? */
	    return FALSE;

	/* Check second magic character. */
	/* gzipped ($1f$8b) or compressed ($1f$9d) */
	if (gzip && ((buff[1] == (char)0x8b) || (buff[1] == (char)0x9d))) {
	    cargv[0] = "gzip";
	    cargv[1] = "-d";
	    cargv[2] = NULL;
	    lseek(*fdp, 0, 0); /* Back to the beginning */
	    *fdp = StartChild(*fdp, _PATH_GZIP, cargv);
	    if (*fdp < 0)
	        return FALSE;
	    (*countp)++;
	    SawCunbatch = TRUE;
	    continue;
	}
	if (buff[1] != RNEWS_MAGIC2)
	    return HadCount ? FALSE : ReadRemainder(*fdp, buff[0], buff[1]);

	/* Some kind of batch -- get the command. */
	if (!ReadLine(&buff[2], (int)(sizeof buff - 3), *fdp))
	    return FALSE;

	if (strncmp(buff, "#! rnews ", 9) == 0) {
	    artsize = atoi(&buff[9]);
	    if (artsize <= 0) {
		syslog(L_ERROR, "bad_line bad count %s", buff);
		return FALSE;
	    }
	    HadCount = TRUE;
	    if (ReadBytecount(*fdp, artsize))
		continue;
	    return FALSE;
	}

	if (HadCount)
	    /* Already saw a bytecount -- probably corrupted. */
	    return FALSE;

	if (strcmp(buff, "#! cunbatch") == 0) {
	    if (SawCunbatch) {
		syslog(L_ERROR, "nested_cunbatch");
		return FALSE;
	    }
	    cargv[0] = UNPACK;
	    cargv[1] = "-d";
	    cargv[2] = NULL;
	    *fdp = StartChild(*fdp, _PATH_GZIP, cargv);
	    if (*fdp < 0)
		return FALSE;
	    (*countp)++;
	    SawCunbatch = TRUE;
	    continue;
	}

#if	defined(DO_RNEWSPROGS)
	cargv[0] = UNPACK;
	cargv[1] = NULL;
	/* Ignore any possible leading pathnames, to avoid trouble. */
	if ((p = strrchr(&buff[3], '/')) != NULL)
	    p++;
	else
	    p = &buff[3];
	if (strchr(_PATH_RNEWSPROGS, '/') == NULL) {
	    snprintf(path, sizeof(path), "%s/%s/%s", innconf->pathbin,
                     _PATH_RNEWSPROGS, p);
	    len = strlen(innconf->pathbin) + 1 + sizeof _PATH_RNEWSPROGS;
	} else {
	    snprintf(path, sizeof(path), "%s/%s", _PATH_RNEWSPROGS, p);
	    len = sizeof _PATH_RNEWSPROGS;
	}
	for (p = &path[len]; *p; p++)
	    if (ISWHITE(*p)) {
		*p = '\0';
		break;
	    }
	*fdp = StartChild(*fdp, path, cargv);
	if (*fdp < 0)
	    return FALSE;
	(*countp)++;
	continue;
#else
	syslog(L_ERROR, "bad_format unknown command %s", buff);
	return FALSE;
#endif	/* defined(DO_RNEWSPROGS) */
    }
    return TRUE;
}


/*
**  Read all articles in the spool directory and unpack them.  Print all
**  errors with xperror as well as syslog, since we're probably being run
**  interactively.
*/
static void
Unspool(void)
{
    register DIR	*dp;
    struct dirent       *ep;
    register bool	ok;
    struct stat		Sb;
    char		hostname[10];
    int			fd;
    size_t		i;
    char                *badname;

    /* Go to the spool directory, get ready to scan it. */
    if (chdir(innconf->pathincoming) < 0) {
	xperror(innconf->pathincoming);
	syslog(L_FATAL, "cant cd %s %m", innconf->pathincoming);
	exit(1);
    }
    if ((dp = opendir(".")) == NULL) {
	xperror("Can't open spool directory");
	syslog(L_FATAL, "cant opendir . %m");
	exit(1);
    }

    /* Loop over all files, and parse them. */
    while ((ep = readdir(dp)) != NULL) {
	InputFile = ep->d_name;
	if (InputFile[0] == '.')
	    continue;
	if (stat(InputFile, &Sb) < 0 && errno != ENOENT) {
	    xperror(InputFile);
	    syslog(L_ERROR, "cant stat %s %m", InputFile);
	    continue;
	}

	if (!S_ISREG(Sb.st_mode))
	    continue;

	if ((fd = open(InputFile, O_RDONLY)) < 0) {
	    if (errno != ENOENT) {
		xperror(InputFile);
		syslog(L_ERROR, "cant open %s %m", InputFile);
	    }
	    continue;
	}
	/* Get UUCP host from spool file, deleting the mktemp XXXXXX suffix. */
	if ((i = strlen(InputFile)) > 6)
	    i -= 6;
	if (i > sizeof hostname - 1)
	    /* Just in case someone wrote their own spooled file. */
	    i = sizeof hostname - 1;
	(void)strncpy(hostname, InputFile, i);
	hostname[i] = '\0';
	UUCPHost = hostname;
	ok = UnpackOne(&fd, &i);
	(void)close(fd);
	WaitForChildren(i);

	if (!ok) {
            badname = concat(PathBadNews, "/XXXXXX", (char *) 0);
            fd = mkstemp(badname);
            if (fd < 0) {
                xperror("Can't create temporary file");
                syslog(L_FATAL, "cant create temporary file %m");
                exit(1);
            }
            close(fd);
	    fprintf(stderr, "Unspooling failed saving to %s\n", badname);
	    syslog(L_ERROR, "cant unspool saving to %s", badname);
	    if (rename(InputFile, badname) < 0) {
		xperror(badname);
		syslog(L_FATAL, "cant rename %s to %s %m", InputFile, badname);
		exit(1);
	    }
	    continue;
	}

	if (unlink(InputFile) < 0)
	    syslog(L_ERROR, "cant remove %s %m", InputFile);
    }
    (void)closedir(dp);
}



/*
**  Can't connect to the server, so spool our input.  There isn't much
**  we can do if this routine fails, unfortunately.  Perhaps try to use
**  an alternate filesystem?
*/
static void
Spool(int fd, int mode)
{
    int spfd;
    int i;
    int j;
    char *tmpspool, *spoolfile, *p;
    char buff[BUFSIZ];
    int count;
    int status;

    if (mode == 'N')
	exit(9);
    tmpspool = concatpath(innconf->pathincoming, ".XXXXXX");
    spfd = mkstemp(tmpspool);
    if (spfd < 0) {
        syslog(L_FATAL, "cant create temporary batch file %m");
        exit(1);
    }
    if (fchmod(spfd, BATCHFILE_MODE) < 0) {
        syslog(L_FATAL, "cant chmod temporary batch file %s %m", tmpspool);
        exit(1);
    }

    /* Read until we there is nothing left. */
    for (status = 0, count = 0; (i = read(fd, buff, sizeof buff)) != 0; ) {
	/* Break out on error. */
	if (i < 0) {
	    syslog(L_FATAL, "cant read after %d %m", count);
	    status++;
	    break;
	}
	/* Write out what we read. */
	for (count += i, p = buff; i; p += j, i -= j)
	    if ((j = write(spfd, p, i)) <= 0) {
		syslog(L_FATAL, "cant write around %d %m", count);
		status++;
		break;
	    }
    }

    /* Close the file. */
    if (close(spfd) < 0) {
	syslog(L_FATAL, "cant close spooled article %s %m", tmpspool);
	status++;
    }

    /* Move temp file into the spool area, and exit appropriately. */
    spoolfile = concatpath(innconf->pathincoming, "XXXXXX");
    spfd = mkstemp(spoolfile);
    if (spfd < 0) {
        syslog(L_FATAL, "cant create spool file %m");
        status++;
    } else {
        close(spfd);
        if (rename(tmpspool, spoolfile) < 0) {
            syslog(L_FATAL, "cant rename %s to %s %m", tmpspool, spoolfile);
            status++;
        }
    }
    free(tmpspool);
    free(spoolfile);
    exit(status);
    /* NOTREACHED */
}


/*
**  Try to read the password file and open a connection to a remote
**  NNTP server.
*/
static bool OpenRemote(char *server, int port, char *buff)
{
    int		i;

    /* Open the remote connection. */
    if (server)
	i = NNTPconnect(server, port, &FromServer, &ToServer, buff);
    else
	i = NNTPremoteopen(port, &FromServer, &ToServer, buff);
    if (i < 0)
	return FALSE;

    *buff = '\0';
    if (NNTPsendpassword(server, FromServer, ToServer) < 0) {
	int oerrno = errno;
	(void)fclose(FromServer);
	(void)fclose(ToServer);
	errno = oerrno;
	return FALSE;
    }
    return TRUE;
}


/*
**  Can't connect to server; print message and spool if necessary.
*/
static void
CantConnect(char *buff, int mode, int fd)
{
    if (buff[0])
	syslog(L_NOTICE, "rejected connection %s", REMclean(buff));
    else
	syslog(L_FATAL, "cant open_remote %m");
    if (mode != 'U')
	Spool(fd, mode);
    exit(1);
}


/*
**  Log an incorrect usage.
*/
static void
Usage(void)
{
    syslog(L_FATAL, "usage error");
    exit(1);
}


int main(int ac, char *av[])
{
    int		fd;
    int		i;
    size_t	count;
    int		mode;
    char	buff[SMBUF];
    int         port = NNTP_PORT;

    /* First thing, set up logging and our identity. */
    openlog("rnews", L_OPENLOG_FLAGS, LOG_INN_PROG);
#if !defined(__FreeBSD__) && !defined(__bsdi__) && !defined (__NetBSD__)
    if (setgid(getegid()) < 0) {
	syslog(L_FATAL, "cant setgid to %d %m", getegid());
	exit(1);
    }
    if (setuid(geteuid()) < 0) {
	syslog(L_FATAL, "cant setuid to %d %m", geteuid());
	exit(1);
    }
#endif
     if (ReadInnConf() < 0) exit(1);
     UUCPHost = getenv(_ENV_UUCPHOST);
     PathBadNews = concatpath(innconf->pathincoming, _PATH_BADNEWS);
     port = innconf->nnrpdpostport;

    (void)umask(NEWSUMASK);

    /* Parse JCL. */
    fd = STDIN_FILENO;
    mode = '\0';
    while ((i = getopt(ac, av, "h:P:NUvr:S:")) != EOF)
	switch (i) {
	default:
	    Usage();
	    /* NOTRTEACHED */
	case 'h':
	    UUCPHost = *optarg ? optarg : NULL;
	    break;
	case 'N':
	case 'U':
	    mode = i;
	    break;
	case 'P':
	    port = atoi(optarg);
	    break;
	case 'v':
	    Verbose = TRUE;
	    break;
	case 'r':
	case 'S':
	    remoteServer = optarg;
	    break;
	}
    ac -= optind;
    av += optind;

    /* Parse arguments.  At most one, the input file. */
    switch (ac) {
    default:
	Usage();
	/* NOTREACHED */
    case 0:
	break;
    case 1:
	if (mode == 'U')
	    Usage();
	if (freopen(av[0], "r", stdin) == NULL) {
	    syslog(L_FATAL, "cant freopen %s %m", av[0]);
	    exit(1);
	}
	fd = fileno(stdin);
	InputFile = av[0];
	break;
    }

    /* Open the link to the server. */
    if (remoteServer != NULL) {
	if (!OpenRemote(remoteServer,port,buff))
		CantConnect(buff,mode,fd);
    } else if (innconf->nnrpdposthost != NULL) {
	if (!OpenRemote(innconf->nnrpdposthost,
	    (port != NNTP_PORT) ? port : innconf->nnrpdpostport, buff))
		CantConnect(buff, mode, fd);
    }
    else {
#if	defined(DO_RNEWSLOCALCONNECT)
	if (NNTPlocalopen(&FromServer, &ToServer, buff) < 0) {
	    /* If server rejected us, no point in continuing. */
	    if (buff[0])
		CantConnect(buff, mode, fd);
	    if (!OpenRemote((char *)NULL,
	    	(port != NNTP_PORT) ? port : innconf->port, buff))
			CantConnect(buff, mode, fd);
	}
#else
	if (!OpenRemote((char *)NULL, 
	    (port != NNTP_PORT) ? port : innconf->port, buff))
		CantConnect(buff, mode, fd);
#endif	/* defined(DO_RNEWSLOCALCONNECT) */
    }
    close_on_exec(fileno(FromServer), true);
    close_on_exec(fileno(ToServer), true);

    /* Execute the command. */
    if (mode == 'U')
	Unspool();
    else {
	if (!UnpackOne(&fd, &count))
	    Spool(fd, mode);
	WaitForChildren(count);
    }

    /* Tell the server we're quitting, get his okay message. */
    (void)fprintf(ToServer, "quit\r\n");
    (void)fflush(ToServer);
    (void)fgets(buff, sizeof buff, FromServer);

    /* Return the appropriate status. */
    exit(0);
    /* NOTREACHED */
}
