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

#include "inn/innconf.h"
#include "inn/messages.h"
#include "inn/wire.h"
#include "libinn.h"
#include "nntp.h"
#include "paths.h"
#include "storage.h"


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
    if (pipe(pan) < 0)
        sysdie("cannot pipe for %s", path);

    /* Get a child. */
    for (i = 0; (pid = fork()) < 0; i++) {
	if (i == innconf->maxforks) {
            syswarn("cannot fork %s, spooling", path);
	    return -1;
	}
        notice("cannot fork %s, waiting", path);
	sleep(60);
    }

    /* Run the child, with redirection. */
    if (pid == 0) {
	close(pan[PIPE_READ]);

	/* Stdin comes from our old input. */
	if (fd != STDIN_FILENO) {
	    if ((i = dup2(fd, STDIN_FILENO)) != STDIN_FILENO) {
                syswarn("cannot dup2 %d to 0, got %d", fd, i);
		_exit(1);
	    }
	    close(fd);
	}

	/* Stdout goes down the pipe. */
	if (pan[PIPE_WRITE] != STDOUT_FILENO) {
	    if ((i = dup2(pan[PIPE_WRITE], STDOUT_FILENO)) != STDOUT_FILENO) {
                syswarn("cannot dup2 %d to 1, got %d", pan[PIPE_WRITE], i);
		_exit(1);
	    }
	    close(pan[PIPE_WRITE]);
	}

	execv(path, (char * const *)argv);
        syswarn("cannot execv %s", path);
	_exit(1);
    }

    close(pan[PIPE_WRITE]);
    close(fd);
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
                syswarn("cannot wait");
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

    notice(reason, arg);
    if (Verbose) {
	fprintf(stderr, "%s: ", InputFile);
	fprintf(stderr, reason, arg);
	fprintf(stderr, " [%.40s...]\n", article);
    }

#if	defined(DO_RNEWS_SAVE_BAD)
    filename = concat(PathBadNews, "/XXXXXX", (char *) 0);
    fd = mkstemp(filename);
    if (fd < 0) {
        warn("cannot create temporary file");
        return;
    }
    F = fdopen(fd, "w");
    if (F == NULL) {
        warn("cannot fdopen %s", filename);
	return;
    }
    length = strlen(article);
    if (fwrite(article, 1, length, F) != length)
        warn("cannot fwrite to %s", filename);
    if (fclose(F) == EOF)
        warn("cannot close %s", filename);
    free(filename);
#endif	/* defined(DO_RNEWS_SAVE_BAD) */
}


/*
**  Process one article.  Return true if the article was okay; false if the
**  whole batch needs to be saved (such as when the server goes down or if
**  the file is corrupted).
*/
static bool Process(char *article)
{
    HEADER	        *hp;
    const char	        *p;
    size_t              length;
    char                *wirefmt, *q;
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
	return true;

    /* Convert the article to wire format. */
    wirefmt = ToWireFmt(article, strlen(article), &length);

    /* Make sure that all the headers are there, note the ID. */
    for (hp = RequiredHeaders; hp < ARRAY_END(RequiredHeaders); hp++) {
        p = wire_findheader(wirefmt, length, hp->Name);
        if (p == NULL) {
            free(wirefmt);
	    Reject(article, "bad_article missing %s", hp->Name);
	    return false;
	}
	if (IS_MESGID(hp)) {
	    id = p;
	    continue;
	}
#if	!defined(DONT_RNEWS_LOG_DUPS)
	if (IS_PATH(hp)) {
	    strlcpy(path, p, sizeof(path));
	    if ((q = strchr(path, '\r')) != NULL)
		*q = '\0';
	}
#endif	/* !defined(DONT_RNEWS_LOG_DUPS) */
    }

    /* Send the NNTP "ihave" message. */
    if ((p = strchr(id, '\r')) == NULL) {
        free(wirefmt);
	Reject(article, "bad_article unterminated %s header", "Message-ID");
	return false;
    }
    msgid = xstrndup(id, p - id);
    fprintf(ToServer, "ihave %s\r\n", msgid);
    fflush(ToServer);
    if (UUCPHost)
        notice("offered %s %s", msgid, UUCPHost);
    free(msgid);

    /* Get a reply, see if they want the article. */
    if (fgets(buff, sizeof buff, FromServer) == NULL) {
        free(wirefmt);
        syswarn("cannot fgets after ihave");
	return false;
    }
    REMclean(buff);
    if (!CTYPE(isdigit, buff[0])) {
        free(wirefmt);
        notice("bad_reply after ihave %s", buff);
	return false;
    }
    switch (atoi(buff)) {
    default:
        free(wirefmt);
	Reject(article, "unknown_reply after ihave %s", buff);
	return true;
    case NNTP_RESENDIT_VAL:
        free(wirefmt);
	return false;
    case NNTP_SENDIT_VAL:
	break;
    case NNTP_HAVEIT_VAL:
#if	defined(SYSLOG_RNEWS_LOG_DUPS)
	*p = '\0';
        notice("duplicate %s %s", id, path);
#endif	/* defined(SYSLOG_RNEWS_LOG_DUPS) */
#if	defined(FILE_RNEWS_LOG_DUPS)
	if ((F = fopen(_PATH_RNEWS_DUP_LOG, "a")) != NULL) {
	    *p = '\0';
	    fprintf(F, "duplicate %s %s\n", id, path);
	    fclose(F);
	}
#endif	/* defined(FILE_RNEWS_LOG_DUPS) */
        free(wirefmt);
	return true;
    }

    /* Send the article to the server. */
    if (fwrite(wirefmt, length, 1, ToServer) != 1) {
        free(wirefmt);
        sysnotice("cant sendarticle");
	return false;
    }
    free(wirefmt);

    /* Flush the server buffer. */
    if (fflush(ToServer) == EOF) {
        syswarn("cant fflush after article");
        return false;
    }

    /* Process server reply code. */
    if (fgets(buff, sizeof buff, FromServer) == NULL) {
        syswarn("cant fgets after article");
	return false;
    }
    REMclean(buff);
    if (!CTYPE(isdigit, buff[0])) {
        notice("bad_reply after article %s", buff);
	return false;
    }
    switch (atoi(buff)) {
    default:
        notice("unknown_reply after article %s", buff);
	/* FALLTHROUGH */
    case NNTP_RESENDIT_VAL:
	return false;
    case NNTP_TOOKIT_VAL:
	break;
    case NNTP_REJECTIT_VAL:
	Reject(article, "rejected %s", buff);
	break;
    }
    return true;
}


/*
**  Read the rest of the input as an article.  Just punt to stdio in
**  this case and let it do the buffering.
*/
static bool
ReadRemainder(int fd, char first, char second)
{
    FILE	*F;
    char	*article;
    char       *p;
    int	size;
    int	used;
    int	left;
    int	i;
    bool		ok;

    /* Turn the descriptor into a stream. */
    if ((i = dup(fd)) < 0 || (F = fdopen(i, "r")) == NULL)
        sysdie("cannot fdopen %d", fd);

    /* Get an initial allocation, leaving space for the \0. */
    size = BUFSIZ + 1;
    article = xmalloc(size + 2);
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
            article = xrealloc(article, size);
        }
        p = fgets(article + used, left, F);
    }
    if (!feof(F))
        sysdie("cannot fgets after %d bytes", used);

    if (article[used - 1] != '\n')
	article[used++] = '\n';
    article[used] = '\0';
    fclose(F);

    ok = Process(article);
    free(article);
    return ok;
}


/*
**  Read an article from the input stream that is artsize bytes long.
*/
static bool
ReadBytecount(int fd, int artsize)
{
    static char		*article;
    static int		oldsize;
    char	*p;
    int	left;
    int	i;

    /* If we haven't gotten any memory before, or we didn't get enough,
     * then get some. */
    if (article == NULL) {
	oldsize = artsize;
	article = xmalloc(oldsize + 1 + 1);
    }
    else if (artsize > oldsize) {
	oldsize = artsize;
        article = xrealloc(article, oldsize + 1 + 1);
    }

    /* Read in the article. */
    for (p = article, left = artsize; left; p += i, left -= i)
	if ((i = read(fd, p, left)) <= 0) {
	    i = errno;
            warn("cannot read, wanted %d got %d", artsize, artsize - left);
#if	0
	    /* Don't do this -- if the article gets re-processed we
	     * will end up accepting the truncated version. */
	    artsize = p - article;
	    article[artsize] = '\0';
	    Reject(article, "short read (%s?)", strerror(i));
#endif	/* 0 */
	    return true;
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
            sysdie("cannot read first line, got %s", save);
	}
	if (*p == '\n') {
	    *p = '\0';
	    return true;
	}
    }
    *p = '\0';
    warn("bad_line too long %s", save);
    return false;
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
    for (SawCunbatch = false, HadCount = false; ; ) {
	/* Get the first character. */
	if ((i = read(*fdp, &buff[0], 1)) < 0) {
            syswarn("cannot read first character");
	    return false;
	}
	if (i == 0)
	    break;

	if (buff[0] == 0x1f)
	    gzip = 1;
	else if (buff[0] != '#')
	    /* Not a batch file.  If we already got one count, the batch
	     * is corrupted, else read rest of input as an article. */
	    return HadCount ? false : ReadRemainder(*fdp, buff[0], '\0');

	/* Get the second character. */
	if ((i = read(*fdp, &buff[1], 1)) < 0) {
            syswarn("cannot read second character");
	    return false;
	}
	if (i == 0)
	    /* A one-byte batch? */
	    return false;

	/* Check second magic character. */
	/* gzipped ($1f$8b) or compressed ($1f$9d) */
	if (gzip && ((buff[1] == (char)0x8b) || (buff[1] == (char)0x9d))) {
	    cargv[0] = "gzip";
	    cargv[1] = "-d";
	    cargv[2] = NULL;
	    lseek(*fdp, 0, 0); /* Back to the beginning */
	    *fdp = StartChild(*fdp, _PATH_GZIP, cargv);
	    if (*fdp < 0)
	        return false;
	    (*countp)++;
	    SawCunbatch = true;
	    continue;
	}
	if (buff[1] != '!')
	    return HadCount ? false : ReadRemainder(*fdp, buff[0], buff[1]);

	/* Some kind of batch -- get the command. */
	if (!ReadLine(&buff[2], (int)(sizeof buff - 3), *fdp))
	    return false;

	if (strncmp(buff, "#! rnews ", 9) == 0) {
	    artsize = atoi(&buff[9]);
	    if (artsize <= 0) {
                syswarn("bad_line bad count %s", buff);
		return false;
	    }
	    HadCount = true;
	    if (ReadBytecount(*fdp, artsize))
		continue;
	    return false;
	}

	if (HadCount)
	    /* Already saw a bytecount -- probably corrupted. */
	    return false;

	if (strcmp(buff, "#! cunbatch") == 0) {
	    if (SawCunbatch) {
                syswarn("nested_cunbatch");
		return false;
	    }
	    cargv[0] = UNPACK;
	    cargv[1] = "-d";
	    cargv[2] = NULL;
	    *fdp = StartChild(*fdp, _PATH_GZIP, cargv);
	    if (*fdp < 0)
		return false;
	    (*countp)++;
	    SawCunbatch = true;
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
	    return false;
	(*countp)++;
	continue;
#else
        warn("bad_format unknown command %s", buff);
	return false;
#endif	/* defined(DO_RNEWSPROGS) */
    }
    return true;
}


/*
**  Read all articles in the spool directory and unpack them.  Print all
**  errors with xperror as well as syslog, since we're probably being run
**  interactively.
*/
static void
Unspool(void)
{
    DIR	*dp;
    struct dirent       *ep;
    bool	ok;
    struct stat		Sb;
    char		hostname[10];
    int			fd, lockfd;
    size_t		i;
    char                *badname, *uuhost;

    message_handlers_die(2, message_log_stderr, message_log_syslog_err);
    message_handlers_warn(2, message_log_stderr, message_log_syslog_err);

    /* Go to the spool directory, get ready to scan it. */
    if (chdir(innconf->pathincoming) < 0)
        sysdie("cannot chdir to %s", innconf->pathincoming);
    if ((dp = opendir(".")) == NULL)
        sysdie("cannot open spool directory");

    /* Loop over all files, and parse them. */
    while ((ep = readdir(dp)) != NULL) {
	InputFile = ep->d_name;
	if (InputFile[0] == '.')
	    continue;
	if (stat(InputFile, &Sb) < 0 && errno != ENOENT) {
            syswarn("cannot stat %s", InputFile);
	    continue;
	}

	if (!S_ISREG(Sb.st_mode))
	    continue;

	if ((fd = open(InputFile, O_RDONLY)) < 0) {
	    if (errno != ENOENT)
                syswarn("cannot open %s", InputFile);
	    continue;
	}

	/*
	** Make sure multiple Unspools don't stomp on eachother.
	** Because of stupid POSIX locking semantics, we need to lock
	** on a seperate fd. Otherwise, dup()ing and then close()ing
	** the dup()ed fd removes the lock we're holding (sigh).
	*/
	if ((lockfd = open(InputFile, O_RDONLY)) < 0) {
	    if (errno != ENOENT)
                syswarn("cannot open %s", InputFile);
	    close(fd);
	    continue;
	}
	if (!inn_lock_file(lockfd, INN_LOCK_READ, 0)) {
	    close(lockfd);
	    close(fd);
	    continue;
	}

	/* Get UUCP host from spool file, deleting the mktemp XXXXXX suffix. */
	uuhost = UUCPHost;
	hostname[0] = 0;
	if ((i = strlen(InputFile)) > 6) {
	    i -= 6;
	    if (i > sizeof hostname - 1)
		/* Just in case someone wrote their own spooled file. */
		i = sizeof hostname - 1;
	    strlcpy(hostname, InputFile, i + 1);
	    UUCPHost = hostname;
	}
	ok = UnpackOne(&fd, &i);
	WaitForChildren(i);
	UUCPHost = uuhost;

	if (!ok) {
            badname = concat(PathBadNews, "/", hostname, "XXXXXX", (char *) 0);
            fd = mkstemp(badname);
            if (fd < 0)
                sysdie("cannot create temporary file");
            close(fd);
            warn("cant unspool saving to %s", badname);
	    if (rename(InputFile, badname) < 0)
                sysdie("cannot rename %s to %s", InputFile, badname);
	    close(fd);
	    close(lockfd);
	    continue;
	}

	if (unlink(InputFile) < 0)
            syswarn("cannot remove %s", InputFile);
	close(fd);
	close(lockfd);
    }
    closedir(dp);

    message_handlers_die(1, message_log_syslog_err);
    message_handlers_warn(1, message_log_syslog_err);
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
    tmpspool = concat(innconf->pathincoming, "/.",
		UUCPHost ? UUCPHost : "", "XXXXXX", (char *)0);
    spfd = mkstemp(tmpspool);
    if (spfd < 0)
        sysdie("cannot create temporary batch file %s", tmpspool);
    if (fchmod(spfd, BATCHFILE_MODE) < 0)
        sysdie("cannot chmod temporary batch file %s", tmpspool);

    /* Read until we there is nothing left. */
    for (status = 0, count = 0; (i = read(fd, buff, sizeof buff)) != 0; ) {
	/* Break out on error. */
	if (i < 0) {
            syswarn("cannot read after %d", count);
	    status++;
	    break;
	}
	/* Write out what we read. */
	for (count += i, p = buff; i; p += j, i -= j)
	    if ((j = write(spfd, p, i)) <= 0) {
                syswarn("cannot write around %d", count);
		status++;
		break;
	    }
    }

    /* Close the file. */
    if (close(spfd) < 0) {
        syswarn("cannot close spooled article %s", tmpspool);
	status++;
    }

    /* Move temp file into the spool area, and exit appropriately. */
    spoolfile = concat(innconf->pathincoming, "/",
		UUCPHost ? UUCPHost : "", "XXXXXX", (char *)0);
    spfd = mkstemp(spoolfile);
    if (spfd < 0) {
        syswarn("cannot create spool file %s", spoolfile);
        status++;
    } else {
        close(spfd);
        if (rename(tmpspool, spoolfile) < 0) {
            syswarn("cannot rename %s to %s", tmpspool, spoolfile);
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
	return false;

    *buff = '\0';
    if (NNTPsendpassword(server, FromServer, ToServer) < 0) {
	int oerrno = errno;
	fclose(FromServer);
	fclose(ToServer);
	errno = oerrno;
	return false;
    }
    return true;
}


/*
**  Can't connect to server; print message and spool if necessary.
*/
static void
CantConnect(char *buff, int mode, int fd)
{
    if (buff[0])
        notice("rejected connection %s", REMclean(buff));
    else
        syswarn("cant open_remote");
    if (mode != 'U')
	Spool(fd, mode);
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
    message_program_name = "rnews";
    message_handlers_notice(1, message_log_syslog_notice);
    message_handlers_warn(1, message_log_syslog_err);
    message_handlers_die(1, message_log_syslog_err);

    if (setgid(getegid()) < 0)
        die("cannot setgid to %lu", (unsigned long) getegid());
    if (setuid(geteuid()) < 0)
        die("cannot setuid to %lu", (unsigned long) geteuid());

    if (!innconf_read(NULL))
        exit(1);
     UUCPHost = getenv(_ENV_UUCPHOST);
     PathBadNews = concatpath(innconf->pathincoming, _PATH_BADNEWS);
     port = innconf->nnrpdpostport;

    umask(NEWSUMASK);

    /* Parse JCL. */
    fd = STDIN_FILENO;
    mode = '\0';
    while ((i = getopt(ac, av, "h:P:NUvr:S:")) != EOF)
	switch (i) {
	default:
	    die("usage error");
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
	    Verbose = true;
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
        die("usage error");
	/* NOTREACHED */
    case 0:
	break;
    case 1:
	if (mode == 'U')
            die("usage error");
	if (freopen(av[0], "r", stdin) == NULL)
            sysdie("cannot freopen %s", av[0]);
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
	if (!UnpackOne(&fd, &count)) {
	    lseek(fd, 0, 0);
	    Spool(fd, mode);
	}
	close(fd);
	WaitForChildren(count);
    }

    /* Tell the server we're quitting, get his okay message. */
    fprintf(ToServer, "quit\r\n");
    fflush(ToServer);
    fgets(buff, sizeof buff, FromServer);

    /* Return the appropriate status. */
    exit(0);
    /* NOTREACHED */
}
