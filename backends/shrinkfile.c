/*  $Id$
**
**  Shrink files on line boundaries.
**  Written by Landon Curt Noll <chongo@toad.com>, and placed in the
**  public domain.  Rewritten for INN by Rich Salz.
**  Usage:
**	shrinkfile [-n] [-s size [-m maxsize]] [-v] file...
**	-n		No writes, exit 0 if any file is too large, 1 otherwise
**	-s size		Truncation size (0 default); suffix may be k, m,
**			or g to scale.  Must not be larger than 2^31 - 1.
**	-m maxsize	Maximum size allowed before truncation.  If maxsize
**			<= size, then it is reset to size.  Default == size.
**	-v		Print status line.
**
**  Files will be shrunk an end of line boundary.  In no case will the
**  file be longer than size bytes if it was longer than maxsize bytes.  
**  If the first line is longer than the absolute value of size, the file 
**  will be truncated to zero length.
**
**  The -n flag may be used to determine of any file is too large.  No
**  files will be altered in this mode.
*/

#include "config.h"
#include "clibrary.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <syslog.h>

#include "libinn.h"
#include "macros.h"

#define MAX_SIZE	0x7fffffffUL


static char *program = NULL;	/* our name */

/*
**  Open a safe unique temporary file that will go away when closed.
*/
static FILE *
OpenTemp(void)
{
    FILE	*F;
    char	buff[SMBUF];
    int		i;

    /* Get filename. */
    snprintf(buff, sizeof(buff), "%s/shrinkXXXXXX", innconf->pathtmp);
    (void)mktemp(buff);

    /* Open the file. */
    if ((i = open(buff, O_RDWR | O_CREAT | O_EXCL | O_TRUNC, 0600)) < 0) {
	(void)fprintf(stderr, "%s: Can't make temporary file, %s\n",
		program, strerror(errno));
	exit(1);
    }
    if ((F = fdopen(i, "w+")) == NULL) {
	(void)fprintf(stderr,
	  "%s: Can't fdopen %d, %s\n", program, i, strerror(errno));
	exit(1);
    }
    (void)unlink(buff);
    return F;
}


/*
**  Does file end with \n?  Assume it does on I/O error, to avoid doing I/O.
*/
static int
EndsWithNewline(FILE *F)
{
    int		c;

    if (fseeko(F, 1, SEEK_END) < 0) {
	(void)fprintf(stderr, "%s: Can't seek to end of file, %s\n",
		program, strerror(errno));
	return TRUE;
    }

    /* return the actual character or EOF */
    if ((c = fgetc(F)) == EOF) {
	if (ferror(F))
	    (void)fprintf(stderr, "%s: Can't read last byte, %s\n",
		    program, strerror(errno));
	return TRUE;
    }
    return c == '\n';
}


/*
**  Add a newline to location of a file.
*/
static bool
AppendNewline(char *name)
{
    FILE	*F;

    if ((F = xfopena(name)) == NULL) {
	(void)fprintf(stderr,
	  "%s: Can't add newline, %s\n", program, strerror(errno));
	return FALSE;
    }

    if (fputc('\n', F) == EOF
     || fflush(F) == EOF
     || ferror(F)
     || fclose(F) == EOF) {
	(void)fprintf(stderr,
	  "%s: Can't add newline, %s\n", program, strerror(errno));
	return FALSE;
    }

    return TRUE;
}

/*
**  Just check if it is too big
*/
static bool
TooBig(FILE *F, off_t maxsize)
{
    struct stat	Sb;

    /* Get the file's size. */
    if (fstat((int)fileno(F), &Sb) < 0) {
	(void)fprintf(stderr,
	  "%s: Can't fstat, %s\n", program, strerror(errno));
	return FALSE;
    }

    /* return TRUE if too large */
    return (maxsize > Sb.st_size ? FALSE : TRUE);
}

/*
**  This routine does all the work.
*/
static bool
Process(FILE *F, char *name, off_t size, off_t maxsize, bool *Changedp)
{
    off_t	len;
    FILE	*tmp;
    struct stat	Sb;
    char	buff[BUFSIZ + 1];
    int		c;
    size_t	i;
    bool	err;

    /* Get the file's size. */
    if (fstat((int)fileno(F), &Sb) < 0) {
	(void)fprintf(stderr,
	  "%s: Can't fstat, %s\n", program, strerror(errno));
	return FALSE;
    }
    len = Sb.st_size;

    /* Process a zero size request. */
    if (size == 0 && len > maxsize) {
	if (len > 0) {
	    (void)fclose(F);
	    if ((F = fopen(name, "w")) == NULL) {
		(void)fprintf(stderr,
		  "%s: Can't overwrite, %s\n", program, strerror(errno));
		return FALSE;
	    }
	    (void)fclose(F);
	    *Changedp = TRUE;
	}
	return TRUE;
    }

    /* See if already small enough. */
    if (len <= maxsize) {
	/* Newline already present? */
	if (EndsWithNewline(F)) {
	    (void)fclose(F);
	    return TRUE;
	}

	/* No newline, add it if it fits. */
	if (len < size - 1) {
	    (void)fclose(F);
	    *Changedp = TRUE;
	    return AppendNewline(name);
	}
    }
    else if (!EndsWithNewline(F)) {
	if (!AppendNewline(name)) {
	    (void)fclose(F);
	    return FALSE;
	}
    }

    /* We now have a file that ends with a newline that is bigger than
     * we want.  Starting from {size} bytes from end, move forward
     * until we get a newline. */
    if (fseeko(F, -size, SEEK_END) < 0) {
	(void)fprintf(stderr,
	  "%s: Can't fseeko, %s\n", program, strerror(errno));
	(void)fclose(F);
	return FALSE;
    }

    while ((c = getc(F)) != '\n')
	if (c == EOF) {
	    (void)fprintf(stderr,
	      "%s: Can't read, %s\n", program, strerror(errno));
	    (void)fclose(F);
	    return FALSE;
	}

    /* Copy rest of file to temp. */
    tmp = OpenTemp();
    err = false;
    while ((i = fread(buff, 1, sizeof buff, F)) > 0)
	if (fwrite(buff, 1, i, tmp) != i) {
	    err = true;
	    break;
	}
    if (err) {
	(void)fprintf(stderr, 
	  "%s: Can't copy to temp file, %s\n", program, strerror(errno));
	(void)fclose(F);
	(void)fclose(tmp);
	return FALSE;
    }

    /* Now copy temp back to original file. */
    (void)fclose(F);
    if ((F = fopen(name, "w")) == NULL) {
	(void)fprintf(stderr,
	  "%s: Can't overwrite, %s\n", program, strerror(errno));
	(void)fclose(tmp);
	return FALSE;
    }
    fseeko(tmp, 0, SEEK_SET);

    while ((i = fread(buff, 1, sizeof buff, tmp)) > 0)
	if (fwrite(buff, 1, i, F) != i) {
	    err = true;
	    break;
	}
    if (err) {
	(void)fprintf(stderr,
	  "%s: Can't overwrite file, %s\n", program, strerror(errno));
	(void)fclose(F);
	(void)fclose(tmp);
	return FALSE;
    }

    (void)fclose(F);
    (void)fclose(tmp);
    *Changedp = TRUE;
    return TRUE;
}


/*
**  Convert size argument to numeric value.  Return -1 on error.
*/
static off_t
ParseSize(char *p)
{
    off_t	scale;
    unsigned long	str_num;
    char	*q;

    /* Skip leading spaces */
    while (ISWHITE(*p))
	p++;
    if (*p == '\0')
	return -1;

    /* determine the scaling factor */
    q = &p[strlen(p) - 1];
    switch (*q) {
    default:
	return -1;
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
	scale = 1;
	break;
    case 'k': case 'K':
	scale = 1024;
	*q = '\0';
	break;
    case 'm': case 'M':
	scale = 1024 * 1024;
	*q = '\0';
	break;
    case 'g': case 'G':
	scale = 1024 * 1024 * 1024;
	*q = '\0';
	break;
    }

    /* Convert string to number. */
    if (sscanf(p, "%lud", &str_num) != 1)
	return -1;
    if (str_num > MAX_SIZE / scale) {
	(void)fprintf(stderr, "%s: Size is too big\n", program);
	exit(1);
    }

    return scale * str_num;
}


/*
**  Print usage message and exit.
*/
static void
Usage(void)
{
    fprintf(stderr, "Usage: %s [-n] [ -m maxsize ] [-s size] [-v] file...\n", program);
    exit(1);
}


int
main(int ac, char *av[])
{
    bool	Changed;
    bool	Verbose;
    bool	no_op;
    FILE	*F;
    char	*p;
    int		i;
    off_t	size = 0;
    off_t	maxsize = 0;

    /* First thing, set up logging and our identity. */
    openlog("shrinkfile", L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);

    /* Set defaults. */
    program = av[0];
    Verbose = FALSE;
    no_op = FALSE;
    (void)umask(NEWSUMASK);

    if (ReadInnConf() < 0) exit(1);

    /* Parse JCL. */
    while ((i = getopt(ac, av, "m:s:vn")) != EOF)
	switch (i) {
	default:
	    Usage();
	    /* NOTREACHED */
	case 'n':
	    no_op = TRUE;
	    break;
	case 'm':
	    if ((maxsize = ParseSize(optarg)) < 0)
		Usage();
	    break;
	case 's':
	    if ((size = ParseSize(optarg)) < 0)
		Usage();
	    break;
	case 'v':
	    Verbose = TRUE;
	    break;
	}
    if (maxsize < size) {
	maxsize = size;
    }
    ac -= optind;
    av += optind;
    if (ac == 0)
	Usage();

    while ((p = *av++) != NULL) {
	if ((F = fopen(p, "r")) == NULL) {
	    (void)fprintf(stderr,
	      "%s: Can't open %s, %s\n", program, p, strerror(errno));
	    continue;
	}

	/* -n (no_op) or normal processing */
	if (no_op) {

	    /* check if too big and exit zero if it is */
	    if (TooBig(F, maxsize)) {
		if (Verbose)
		    (void)printf("%s: %s is too large\n", program, p);
		exit(0);
		/* NOTREACHED */
	    }

	/* no -n, do some real work */
	} else {
	    Changed = FALSE;
	    if (!Process(F, p, size, maxsize, &Changed))
		(void)fprintf(stderr, "%s: Can't shrink %s\n", program, p);
	    else if (Verbose && Changed)
		(void)printf("%s: Shrunk %s\n", program, p);
	}
    }
    if (no_op && Verbose) {
	(void)printf("%s: did not find a file that was too large\n", program);
    }

    /* if -n, then exit non-zero to indicate no file too big */
    exit(no_op ? 1 : 0);
    /* NOTREACHED */
}
