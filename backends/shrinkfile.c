/*  $Id$
**
**  Shrink files on line boundaries.
**
**  Written by Landon Curt Noll <chongo@toad.com>, and placed in the
**  public domain.  Rewritten for INN by Rich Salz.
**
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

#include "inn/innconf.h"
#include "inn/messages.h"
#include "libinn.h"

#define MAX_SIZE	0x7fffffffUL


/*
**  Open a safe unique temporary file that will go away when closed.
*/
static FILE *
OpenTemp(void)
{
    FILE	*F;
    char        *filename;
    int		fd;

    filename = concatpath(innconf->pathtmp, "shrinkXXXXXX");
    fd = mkstemp(filename);
    if (fd < 0)
        sysdie("cannot create temporary file");
    F = fdopen(fd, "w+");
    if (F == NULL)
        sysdie("cannot fdopen %s", filename);
    unlink(filename);
    free(filename);
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
        syswarn("cannot seek to end of file");
	return true;
    }

    /* return the actual character or EOF */
    if ((c = fgetc(F)) == EOF) {
	if (ferror(F))
            syswarn("cannot read last byte");
	return true;
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
        syswarn("cannot add newline");
	return false;
    }

    if (fputc('\n', F) == EOF
     || fflush(F) == EOF
     || ferror(F)
     || fclose(F) == EOF) {
        syswarn("cannot add newline");
	return false;
    }

    return true;
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
        syswarn("cannot fstat");
	return false;
    }

    /* return true if too large */
    return (maxsize > Sb.st_size ? false : true);
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
        syswarn("cannot fstat");
	return false;
    }
    len = Sb.st_size;

    /* Process a zero size request. */
    if (size == 0 && len > maxsize) {
	if (len > 0) {
	    fclose(F);
	    if ((F = fopen(name, "w")) == NULL) {
                syswarn("cannot overwrite");
		return false;
	    }
	    fclose(F);
	    *Changedp = true;
	}
	return true;
    }

    /* See if already small enough. */
    if (len <= maxsize) {
	/* Newline already present? */
	if (EndsWithNewline(F)) {
	    fclose(F);
	    return true;
	}

	/* No newline, add it if it fits. */
	if (len < size - 1) {
	    fclose(F);
	    *Changedp = true;
	    return AppendNewline(name);
	}
    }
    else if (!EndsWithNewline(F)) {
	if (!AppendNewline(name)) {
	    fclose(F);
	    return false;
	}
    }

    /* We now have a file that ends with a newline that is bigger than
     * we want.  Starting from {size} bytes from end, move forward
     * until we get a newline. */
    if (fseeko(F, -size, SEEK_END) < 0) {
        syswarn("cannot fseeko");
	fclose(F);
	return false;
    }

    while ((c = getc(F)) != '\n')
	if (c == EOF) {
            syswarn("cannot read");
	    fclose(F);
	    return false;
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
        syswarn("cannot copy to temporary file");
	fclose(F);
	fclose(tmp);
	return false;
    }

    /* Now copy temp back to original file. */
    fclose(F);
    if ((F = fopen(name, "w")) == NULL) {
        syswarn("cannot overwrite file");
	fclose(tmp);
	return false;
    }
    fseeko(tmp, 0, SEEK_SET);

    while ((i = fread(buff, 1, sizeof buff, tmp)) > 0)
	if (fwrite(buff, 1, i, F) != i) {
	    err = true;
	    break;
	}
    if (err) {
        syswarn("cannot overwrite file");
	fclose(F);
	fclose(tmp);
	return false;
    }

    fclose(F);
    fclose(tmp);
    *Changedp = true;
    return true;
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
    if (str_num > MAX_SIZE / scale)
        die("size is too big");

    return scale * str_num;
}


/*
**  Print usage message and exit.
*/
static void
Usage(void)
{
    fprintf(stderr,
            "Usage: shrinkfile [-n] [ -m maxsize ] [-s size] [-v] file...");
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

    /* First thing, set up our identity. */
    message_program_name = "shrinkfile";

    /* Set defaults. */
    Verbose = false;
    no_op = false;
    umask(NEWSUMASK);

    if (!innconf_read(NULL))
        exit(1);

    /* Parse JCL. */
    while ((i = getopt(ac, av, "m:s:vn")) != EOF)
	switch (i) {
	default:
	    Usage();
	    /* NOTREACHED */
	case 'n':
	    no_op = true;
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
	    Verbose = true;
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
            syswarn("cannot open %s", p);
	    continue;
	}

	/* -n (no_op) or normal processing */
	if (no_op) {

	    /* check if too big and exit zero if it is */
	    if (TooBig(F, maxsize)) {
		if (Verbose)
                    notice("%s is too large", p);
		exit(0);
		/* NOTREACHED */
	    }

	/* no -n, do some real work */
	} else {
	    Changed = false;
	    if (!Process(F, p, size, maxsize, &Changed))
                syswarn("cannot shrink %s", p);
	    else if (Verbose && Changed)
                notice("shrunk %s", p);
	}
    }
    if (no_op && Verbose) {
        notice("did not find a file that was too large");
    }

    /* if -n, then exit non-zero to indicate no file too big */
    exit(no_op ? 1 : 0);
    /* NOTREACHED */
}
