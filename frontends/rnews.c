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
#include "inn/libinn.h"
#include "inn/messages.h"
#include "inn/newsuser.h"
#include "inn/nntp.h"
#include "inn/paths.h"
#include "inn/storage.h"
#include "inn/wire.h"


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
	if (i == (long) innconf->maxforks) {
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
static void
Reject(const char *article, size_t length UNUSED, const char *reason,
       const char *arg)
{
#if	defined(DO_RNEWS_SAVE_BAD)
    char *filename;
    FILE *F;
    int fd;
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
static bool
Process(char *article, size_t artlen)
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
    wirefmt = wire_from_native(article, artlen, &length);

    /* Make sure that all the headers are there, note the ID. */
    for (hp = RequiredHeaders; hp < ARRAY_END(RequiredHeaders); hp++) {
        p = wire_findheader(wirefmt, length, hp->Name, true);
        if (p == NULL) {
            free(wirefmt);
	    Reject(article, artlen, "bad_article missing %s", hp->Name);
	    return true;
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
	Reject(article, artlen, "bad_article unterminated %s header",
               "Message-ID");
	return true;
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
        if (ferror(FromServer))
            syswarn("cannot fgets after ihave");
        else
            warn("unexpected EOF from server after ihave");
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
        notice("unknown_reply after ihave %s", buff);
	return false;
    case NNTP_FAIL_IHAVE_DEFER:
        free(wirefmt);
	return false;
    case NNTP_CONT_IHAVE:
	break;
    case NNTP_FAIL_IHAVE_REFUSE:
#if	defined(SYSLOG_RNEWS_LOG_DUPS)
	*p = '\0';
        notice("duplicate %s %s", id, path);
#endif	/* defined(SYSLOG_RNEWS_LOG_DUPS) */
#if	defined(FILE_RNEWS_LOG_DUPS)
	if ((F = fopen(INN_PATH_RNEWS_DUP_LOG, "a")) != NULL) {
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
        if (ferror(FromServer))
            syswarn("cannot fgets after article");
        else
            warn("unexpected EOF from server after article");
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
    case NNTP_FAIL_IHAVE_DEFER:
	return false;
    case NNTP_OK_IHAVE:
	break;
    case NNTP_FAIL_IHAVE_REJECT:
	Reject(article, artlen, "rejected %s", buff);
	break;
    }
    return true;
}


/*
**  Read the rest of the input as an article.
*/
static bool
ReadRemainder(int fd, char first, char second)
{
    char	*article;
    char	*p;
    char	buf[BUFSIZ];
    int		size;
    int		used;
    int		left;
    int		skipnl;
    int		i, n;
    bool	ok;

    /* Get an initial allocation, leaving space for the \0. */
    size = BUFSIZ + 1;
    article = xmalloc(size + 2);
    article[0] = first;
    article[1] = second;
    used = second ? 2 : 1;
    left = size - used;
    skipnl = 0;

    /* Read the input, coverting line ends as we go if necessary. */
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
	p = article + used;
	for (i = 0; i < n; i++) {
	    if (skipnl) {
		skipnl = 0;
		if (buf[i] == '\n') continue;
	    }
	    if (buf[i] == '\r') {
		buf[i] = '\n';
		skipnl = 1;
	    }
	    *p++ = buf[i];
	    used++;
	    left--;
	    if (left < SMBUF) {
		size += BUFSIZ;
		left += BUFSIZ;
		article = xrealloc(article, size);
		p = article + used;
	    }
	}
    }
    if (n < 0)
        sysdie("cannot read after %d bytes", used);

    if (article[used - 1] != '\n')
	article[used++] = '\n';
    article[used] = '\0';

    ok = Process(article, used);
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

    /* Read in the article.  If we don't get as many bytes as we should,
       return true without doing anything, throwing away the article.  This
       seems like the best of a set of bad options; Reject would save the
       article into bad and then someone might reprocess it, leaving us with
       accepting the truncated version. */
    for (p = article, left = artsize; left; p += i, left -= i)
	if ((i = read(fd, p, left)) <= 0) {
	    i = errno;
            warn("cannot read, wanted %d got %d", artsize, artsize - left);
	    return true;
	}
    if (p[-1] != '\n') {
	*p++ = '\n';
        artsize++;
    }
    *p = '\0';

    return Process(article, artsize);
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
	    *fdp = StartChild(*fdp, INN_PATH_GZIP, cargv);
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
	    *fdp = StartChild(*fdp, INN_PATH_GZIP, cargv);
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
	if (strchr(INN_PATH_RNEWSPROGS, '/') == NULL) {
	    snprintf(path, sizeof(path), "%s/%s/%s", innconf->pathbin,
                     INN_PATH_RNEWSPROGS, p);
	    len = strlen(innconf->pathbin) + 1 + sizeof INN_PATH_RNEWSPROGS;
	} else {
	    snprintf(path, sizeof(path), "%s/%s", INN_PATH_RNEWSPROGS, p);
	    len = sizeof INN_PATH_RNEWSPROGS;
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
    int			fd;
    size_t		i;
    char                *uuhost;

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

	if ((fd = open(InputFile, O_RDWR)) < 0) {
	    if (errno != ENOENT)
                syswarn("cannot open %s", InputFile);
	    continue;
	}

	/* Make sure multiple Unspools don't stomp on eachother. */
	if (!inn_lock_file(fd, INN_LOCK_WRITE, 0)) {
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

        /* If UnpackOne returned true, the article has been dealt with one way
           or the other, so remove it.  Otherwise, leave it in place; either
           we got an unknown error from the server or we got a deferral, and
           for both we want to try later. */
	if (ok) {
            if (unlink(InputFile) < 0)
                syswarn("cannot remove %s", InputFile);
        }

	close(fd);
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
static bool OpenRemote(char *server, int port, char *buff, size_t len)
{
    int		i;

    /* Open the remote connection. */
    if (server)
	i = NNTPconnect(server, port, &FromServer, &ToServer, buff, len);
    else
	i = NNTPremoteopen(port, &FromServer, &ToServer, buff, len);
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

    /* The reason for the following is somewhat obscure and is done only
       because rnews is sometimes installed setuid.

       The stderr stream used by message_log_syslog_err is associated with
       file descriptor 2, generally even if that file descriptor is closed.
       Someone running rnews may close all of the standard file descriptors
       before running it, in which case, later in its operations, one of the
       article files or network connections it has open could be file
       descriptor 2.  If an error occurs at that point, the error message may
       be written to that file or network connection instead of to stderr,
       with unpredictable results.

       We avoid this by burning three file descriptors if the real and
       effective user IDs don't match, or if we're running as root.  (If they
       do match, there is no escalation of privileges and at worst the user is
       just managing to produce a strange bug.) */
    if (getuid() != geteuid() || geteuid() == 0) {
        if (open("/dev/null", O_RDONLY) < 0)
            sysdie("cannot open /dev/null");
        if (open("/dev/null", O_RDONLY) < 0)
            sysdie("cannot open /dev/null");
        if (open("/dev/null", O_RDONLY) < 0)
            sysdie("cannot open /dev/null");
    }

    /* Make sure that we switch to the news user if we're running as root,
       since we may spool files and don't want those files owned by root.
       Don't require that we be running as the news user, though; there are
       other setups where rnews might be setuid news or be run by other
       processes in the news group. */
    if (getuid() == 0 || geteuid() == 0) {
        uid_t uid;

        get_news_uid_gid(&uid, false, true);
        setuid(uid);
    }

    if (!innconf_read(NULL))
        exit(1);
    UUCPHost = getenv(INN_ENV_UUCPHOST);
    PathBadNews = concatpath(innconf->pathincoming, INN_PATH_BADNEWS);
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
	if (!OpenRemote(remoteServer, port, buff, sizeof(buff)))
		CantConnect(buff,mode,fd);
    } else if (innconf->nnrpdposthost != NULL) {
	if (!OpenRemote(innconf->nnrpdposthost,
                        (port != NNTP_PORT) ? (unsigned) port : innconf->nnrpdpostport,
                        buff, sizeof(buff)))
		CantConnect(buff, mode, fd);
    }
    else {
	if (NNTPlocalopen(&FromServer, &ToServer, buff, sizeof(buff)) < 0) {
	    /* If server rejected us, no point in continuing. */
	    if (buff[0])
		CantConnect(buff, mode, fd);
	    if (!OpenRemote(NULL, (port != NNTP_PORT) ? (unsigned) port : innconf->port,
                            buff, sizeof(buff)))
			CantConnect(buff, mode, fd);
	}
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
